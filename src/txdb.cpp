// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "txdb.h"

#include "chainparams.h"
#include "hash.h"
#include "pow.h"
#include "uint256.h"
#include "main.h"
#include "consensus/consensus.h"
#include "base58.h"

#include <stdint.h>

#include <boost/thread.hpp>

using namespace std;

static const char DB_COINS = 'c';
static const char DB_BLOCK_FILES = 'f';
static const char DB_TXAPOLLON = 't';
static const char DB_ADDRESSAPOLLON = 'a';
static const char DB_ADDRESSUNSPENTAPOLLON = 'u';
static const char DB_TIMESTAMPAPOLLON = 's';
static const char DB_SPENTAPOLLON = 'p';
static const char DB_BLOCK_APOLLON = 'b';

static const char DB_BEST_BLOCK = 'B';
static const char DB_FLAG = 'F';
static const char DB_REAPOLLON_FLAG = 'R';
static const char DB_LAST_BLOCK = 'l';
static const char DB_TOTAL_SUPPLY = 'S';


CCoinsViewDB::CCoinsViewDB(size_t nCacheSize, bool fMemory, bool fWipe) : db(GetDataDir() / "chainstate", nCacheSize, fMemory, fWipe, true)
{
}

bool CCoinsViewDB::GetCoins(const uint256 &txid, CCoins &coins) const {
    return db.Read(make_pair(DB_COINS, txid), coins);
}

bool CCoinsViewDB::HaveCoins(const uint256 &txid) const {
    return db.Exists(make_pair(DB_COINS, txid));
}

uint256 CCoinsViewDB::GetBestBlock() const {
    uint256 hashBestChain;
    if (!db.Read(DB_BEST_BLOCK, hashBestChain))
        return uint256();
    return hashBestChain;
}

bool CCoinsViewDB::BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock) {
    CDBBatch batch(db);
    size_t count = 0;
    size_t changed = 0;
    for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end();) {
        if (it->second.flags & CCoinsCacheEntry::DIRTY) {
            if (it->second.coins.IsPruned())
                batch.Erase(make_pair(DB_COINS, it->first));
            else
                batch.Write(make_pair(DB_COINS, it->first), it->second.coins);
            changed++;
        }
        count++;
        CCoinsMap::iterator itOld = it++;
        mapCoins.erase(itOld);
    }
    if (!hashBlock.IsNull())
        batch.Write(DB_BEST_BLOCK, hashBlock);

    LogPrint("coindb", "Committing %u changed transactions (out of %u) to coin database...\n", (unsigned int)changed, (unsigned int)count);
    return db.WriteBatch(batch);
}

CBlockTreeDB::CBlockTreeDB(size_t nCacheSize, bool fMemory, bool fWipe) : CDBWrapper(GetDataDir() / "blocks" / "apollon", nCacheSize, fMemory, fWipe) {
}

bool CBlockTreeDB::ReadBlockFileInfo(int nFile, CBlockFileInfo &info) {
    return Read(make_pair(DB_BLOCK_FILES, nFile), info);
}

bool CBlockTreeDB::WriteReapolloning(bool fReapolloning) {
    if (fReapolloning)
        return Write(DB_REAPOLLON_FLAG, '1');
    else
        return Erase(DB_REAPOLLON_FLAG);
}

bool CBlockTreeDB::ReadReapolloning(bool &fReapolloning) {
    fReapolloning = Exists(DB_REAPOLLON_FLAG);
    return true;
}

bool CBlockTreeDB::ReadLastBlockFile(int &nFile) {
    return Read(DB_LAST_BLOCK, nFile);
}

CCoinsViewCursor *CCoinsViewDB::Cursor() const
{
    CCoinsViewDBCursor *i = new CCoinsViewDBCursor(const_cast<CDBWrapper*>(&db)->NewIterator(), GetBestBlock());
    /* It seems that there are no "const iterators" for LevelDB.  Since we
       only need read operations on it, use a const-cast to get around
       that restriction.  */
    i->pcursor->Seek(DB_COINS);
    // Cache key of first record
    i->pcursor->GetKey(i->keyTmp);
    return i;
}

bool CCoinsViewDBCursor::GetKey(uint256 &key) const
{
    // Return cached key
    if (keyTmp.first == DB_COINS) {
        key = keyTmp.second;
        return true;
    }
    return false;
}

bool CCoinsViewDBCursor::GetValue(CCoins &coins) const
{
    return pcursor->GetValue(coins);
}

unsigned int CCoinsViewDBCursor::GetValueSize() const
{
    return pcursor->GetValueSize();
}

bool CCoinsViewDBCursor::Valid() const
{
    return keyTmp.first == DB_COINS;
}

void CCoinsViewDBCursor::Next()
{
    pcursor->Next();
    if (!pcursor->Valid() || !pcursor->GetKey(keyTmp))
        keyTmp.first = 0; // Invalidate cached key after last record so that Valid() and GetKey() return false
}

bool CBlockTreeDB::WriteBatchSync(const std::vector<std::pair<int, const CBlockFileInfo*> >& fileInfo, int nLastFile, const std::vector<const CBlockApollon*>& blockinfo) {
    CDBBatch batch(*this);
    for (std::vector<std::pair<int, const CBlockFileInfo*> >::const_iterator it=fileInfo.begin(); it != fileInfo.end(); it++) {
        batch.Write(make_pair(DB_BLOCK_FILES, it->first), *it->second);
    }
    batch.Write(DB_LAST_BLOCK, nLastFile);
    for (std::vector<const CBlockApollon*>::const_iterator it=blockinfo.begin(); it != blockinfo.end(); it++) {
    	batch.Write(make_pair(DB_BLOCK_APOLLON, (*it)->GetBlockHash()), CDiskBlockApollon(*it));
    }
    return WriteBatch(batch, true);
}

bool CBlockTreeDB::ReadTxApollon(const uint256 &txid, CDiskTxPos &pos) {
    return Read(make_pair(DB_TXAPOLLON, txid), pos);
}

bool CBlockTreeDB::WriteTxApollon(const std::vector<std::pair<uint256, CDiskTxPos> >&vect) {
    CDBBatch batch(*this);
    for (std::vector<std::pair<uint256,CDiskTxPos> >::const_iterator it=vect.begin(); it!=vect.end(); it++)
        batch.Write(make_pair(DB_TXAPOLLON, it->first), it->second);
    return WriteBatch(batch);
}

bool CBlockTreeDB::ReadSpentApollon(CSpentApollonKey &key, CSpentApollonValue &value) {
    return Read(make_pair(DB_SPENTAPOLLON, key), value);
}

bool CBlockTreeDB::UpdateSpentApollon(const std::vector<std::pair<CSpentApollonKey, CSpentApollonValue> >&vect) {
    CDBBatch batch(*this);
    for (std::vector<std::pair<CSpentApollonKey,CSpentApollonValue> >::const_iterator it=vect.begin(); it!=vect.end(); it++) {
        if (it->second.IsNull()) {
            batch.Erase(make_pair(DB_SPENTAPOLLON, it->first));
        } else {
            batch.Write(make_pair(DB_SPENTAPOLLON, it->first), it->second);
        }
    }
    return WriteBatch(batch);
}

bool CBlockTreeDB::UpdateAddressUnspentApollon(const std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue > >&vect) {
    CDBBatch batch(*this);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=vect.begin(); it!=vect.end(); it++) {
        if (it->second.IsNull()) {
            batch.Erase(make_pair(DB_ADDRESSUNSPENTAPOLLON, it->first));
        } else {
            batch.Write(make_pair(DB_ADDRESSUNSPENTAPOLLON, it->first), it->second);
        }
    }
    return WriteBatch(batch);
}

bool CBlockTreeDB::ReadAddressUnspentApollon(uint160 addressHash, AddressType type,
                                           std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &unspentOutputs) {

    boost::scoped_ptr<CDBIterator> pcursor(NewIterator());

    pcursor->Seek(make_pair(DB_ADDRESSUNSPENTAPOLLON, CAddressApollonIteratorKey(type, addressHash)));

    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char,CAddressUnspentKey> key;
        if (pcursor->GetKey(key) && key.first == DB_ADDRESSUNSPENTAPOLLON && key.second.hashBytes == addressHash) {
            CAddressUnspentValue nValue;
            if (pcursor->GetValue(nValue)) {
                unspentOutputs.push_back(make_pair(key.second, nValue));
                pcursor->Next();
            } else {
                return error("failed to get address unspent value");
            }
        } else {
            break;
        }
    }

    return true;
}

bool CBlockTreeDB::WriteAddressApollon(const std::vector<std::pair<CAddressApollonKey, CAmount > >&vect) {
    CDBBatch batch(*this);
    for (std::vector<std::pair<CAddressApollonKey, CAmount> >::const_iterator it=vect.begin(); it!=vect.end(); it++)
    batch.Write(make_pair(DB_ADDRESSAPOLLON, it->first), it->second);
    return WriteBatch(batch);
}

bool CBlockTreeDB::EraseAddressApollon(const std::vector<std::pair<CAddressApollonKey, CAmount > >&vect) {
    CDBBatch batch(*this);
    for (std::vector<std::pair<CAddressApollonKey, CAmount> >::const_iterator it=vect.begin(); it!=vect.end(); it++)
    batch.Erase(make_pair(DB_ADDRESSAPOLLON, it->first));
    return WriteBatch(batch);
}

bool CBlockTreeDB::ReadAddressApollon(uint160 addressHash, AddressType type,
                                    std::vector<std::pair<CAddressApollonKey, CAmount> > &addressApollon,
                                    int start, int end) {

    boost::scoped_ptr<CDBIterator> pcursor(NewIterator());

    if (start > 0 && end > 0) {
        pcursor->Seek(make_pair(DB_ADDRESSAPOLLON, CAddressApollonIteratorHeightKey(type, addressHash, start)));
    } else {
        pcursor->Seek(make_pair(DB_ADDRESSAPOLLON, CAddressApollonIteratorKey(type, addressHash)));
    }

    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char,CAddressApollonKey> key;
        if (pcursor->GetKey(key) && key.first == DB_ADDRESSAPOLLON && key.second.hashBytes == addressHash && key.second.type == type) {
            if (end > 0 && key.second.blockHeight > end) {
                break;
            }
            CAmount nValue;
            if (pcursor->GetValue(nValue)) {
                addressApollon.push_back(make_pair(key.second, nValue));
                pcursor->Next();
            } else {
                return error("failed to get address apollon value");
            }
        } else {
            break;
        }
    }

    return true;
}


bool CBlockTreeDB::WriteTimestampApollon(const CTimestampApollonKey &timestampApollon) {
    CDBBatch batch(*this);
    batch.Write(make_pair(DB_TIMESTAMPAPOLLON, timestampApollon), 0);
    return WriteBatch(batch);
}

bool CBlockTreeDB::ReadTimestampApollon(const unsigned int &high, const unsigned int &low, std::vector<uint256> &hashes) {

    boost::scoped_ptr<CDBIterator> pcursor(NewIterator());

    pcursor->Seek(make_pair(DB_TIMESTAMPAPOLLON, CTimestampApollonIteratorKey(low)));

    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char, CTimestampApollonKey> key;
        if (pcursor->GetKey(key) && key.first == DB_TIMESTAMPAPOLLON && key.second.timestamp <= high) {
            hashes.push_back(key.second.blockHash);
            pcursor->Next();
        } else {
            break;
        }
    }

    return true;
}

bool CBlockTreeDB::WriteFlag(const std::string &name, bool fValue) {
    return Write(std::make_pair(DB_FLAG, name), fValue ? '1' : '0');
}

bool CBlockTreeDB::ReadFlag(const std::string &name, bool &fValue) {
    char ch;
    if (!Read(std::make_pair(DB_FLAG, name), ch))
        return false;
    fValue = ch == '1';
    return true;
}

bool CBlockTreeDB::LoadBlockApollonGuts(boost::function<CBlockApollon*(const uint256&)> insertBlockApollon)
{
    auto consensusParams = Params().GetConsensus();
    LogPrintf("CBlockTreeDB::LoadBlockApollonGuts\n");
    //bool fTestNet = (Params().NetworkIDString() == CBaseChainParams::TESTNET);
    boost::scoped_ptr<CDBIterator> pcursor(NewIterator());

    pcursor->Seek(make_pair(DB_BLOCK_APOLLON, uint256()));

    // Load mapBlockApollon
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char, uint256> key;
        if (pcursor->GetKey(key) && key.first == DB_BLOCK_APOLLON) {
            CDiskBlockApollon diskapollon;
            if (pcursor->GetValue(diskapollon)) {
                // Construct block apollon object
            	//if(diskapollon.hashBlock != uint256()
            	//	&& diskapollon.hashPrev != uint256()){

                CBlockApollon* papollonNew    = insertBlockApollon(diskapollon.GetBlockHash());
                papollonNew->pprev 		  = insertBlockApollon(diskapollon.hashPrev);

                papollonNew->nHeight        = diskapollon.nHeight;
                papollonNew->nFile          = diskapollon.nFile;
                papollonNew->nDataPos       = diskapollon.nDataPos;
                papollonNew->nUndoPos       = diskapollon.nUndoPos;
                papollonNew->nVersion       = diskapollon.nVersion;
                papollonNew->hashMerkleRoot = diskapollon.hashMerkleRoot;
                papollonNew->nTime          = diskapollon.nTime;
                papollonNew->nBits          = diskapollon.nBits;
                papollonNew->nNonce         = diskapollon.nNonce;
                papollonNew->nStatus        = diskapollon.nStatus;
                papollonNew->nTx            = diskapollon.nTx;

                papollonNew->accumulatorChanges = diskapollon.accumulatorChanges;
                papollonNew->mintedPubCoins     = diskapollon.mintedPubCoins;
                papollonNew->spentSerials       = diskapollon.spentSerials;

                papollonNew->sigmaMintedPubCoins   = diskapollon.sigmaMintedPubCoins;
                papollonNew->sigmaSpentSerials     = diskapollon.sigmaSpentSerials;
                papollonNew->nStakeModifier = diskapollon.nStakeModifier;
                papollonNew->vchBlockSig    = diskapollon.vchBlockSig; // qtum

                if (papollonNew->nNonce != 0 && !CheckProofOfWork(papollonNew->GetBlockHash(), papollonNew->nBits, consensusParams))
                        return error("LoadBlockApollon(): CheckProofOfWork failed: %s", papollonNew->ToString());

                pcursor->Next();
            } else {
                return error("LoadBlockApollon() : failed to read value");
            }
        } else {
            break;
        }
    }

    return true;
}

int CBlockTreeDB::GetBlockApollonVersion()
{
    // Get random block apollon entry, check its version. The only reason for these functions to exist
    // is to check if the apollon is from previous version and needs to be rebuilt. Comparison of ANY
    // record version to threshold value would be enough to decide if reapollon is needed.

    return GetBlockApollonVersion(uint256());
}

int CBlockTreeDB::GetBlockApollonVersion(uint256 const & blockHash)
{
    boost::scoped_ptr<CDBIterator> pcursor(NewIterator());
    pcursor->Seek(make_pair(DB_BLOCK_APOLLON, blockHash));
    uint256 const zero_hash = uint256();
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char, uint256> key;
        if (pcursor->GetKey(key) && key.first == DB_BLOCK_APOLLON) {
            if (blockHash != zero_hash && key.second != blockHash) {
                pcursor->Next();
                continue;
            }
            CDiskBlockApollon diskapollon;
            if (pcursor->GetValue(diskapollon))
                return diskapollon.nDiskBlockVersion;
        } else {
	    break;
        }
    }
    return -1;
}


bool CBlockTreeDB::AddTotalSupply(CAmount const & supply)
{
    CAmount current = 0;
    Read(DB_TOTAL_SUPPLY, current);
    current += supply;
    return Write(DB_TOTAL_SUPPLY, current);
}

bool CBlockTreeDB::ReadTotalSupply(CAmount & supply)
{
    CAmount current = 0;
    if(Read(DB_TOTAL_SUPPLY, current)) {
        supply = current;
        return true;
    }
    return false;
}

/******************************************************************************/

CDbApollonHelper::CDbApollonHelper(bool addressApollon_, bool spentApollon_)
{
    if (addressApollon_) {
        addressApollon.reset(AddressApollon());
        addressUnspentApollon.reset(AddressUnspentApollon());
    }

    if (spentApollon_)
        spentApollon.reset(SpentApollon());
}

namespace {

using AddressApollonPtr = boost::optional<CDbApollonHelper::AddressApollon>;
using AddressUnspentApollonPtr = boost::optional<CDbApollonHelper::AddressUnspentApollon>;
using SpentApollonPtr = boost::optional<CDbApollonHelper::SpentApollon>;

std::pair<AddressType, uint160> classifyAddress(txnouttype type, vector<vector<unsigned char> > const & addresses)
{
    std::pair<AddressType, uint160> result(AddressType::unknown, uint160());
    if(type == TX_PUBKEY) {
        result.first = AddressType::payToPubKeyHash;
        CPubKey pubKey(addresses.front().begin(), addresses.front().end());
        result.second = pubKey.GetID();
    } else if(type == TX_SCRIPTHASH) {
        result.first = AddressType::payToScriptHash;
        result.second = uint160(std::vector<unsigned char>(addresses.front().begin(), addresses.front().end()));
    } else if(type == TX_PUBKEYHASH) {
        result.first = AddressType::payToPubKeyHash;
        result.second = uint160(std::vector<unsigned char>(addresses.front().begin(), addresses.front().end()));
    }
    return result;
}

void handleInput(CTxIn const & input, size_t inputNo, uint256 const & txHash, int height, int txNumber, CCoinsViewCache const & view,
        AddressApollonPtr & addressApollon, AddressUnspentApollonPtr & addressUnspentApollon, SpentApollonPtr & spentApollon)
{
    const CCoins* coins = view.AccessCoins(input.prevout.hash);
    const CTxOut &prevout = coins->vout[input.prevout.n];

    txnouttype type;
    vector<vector<unsigned char> > addresses;

    if(!Solver(prevout.scriptPubKey, type, addresses)) {
        LogPrint("CDbApollonHelper", "Encountered an unsoluble script in block:%i, txHash: %s, inputNo: %i\n", height, txHash.ToString().c_str(), inputNo);
        return;
    }

    std::pair<AddressType, uint160> addrType = classifyAddress(type, addresses);

    if(addrType.first == AddressType::unknown) {
        return;
    }

    if (addressApollon) {
        addressApollon->push_back(make_pair(CAddressApollonKey(addrType.first, addrType.second, height, txNumber, txHash, inputNo, true), prevout.nValue * -1));
        addressUnspentApollon->push_back(make_pair(CAddressUnspentKey(addrType.first, addrType.second, input.prevout.hash, input.prevout.n), CAddressUnspentValue()));
    }

    if (spentApollon)
        spentApollon->push_back(make_pair(CSpentApollonKey(input.prevout.hash, input.prevout.n), CSpentApollonValue(txHash, inputNo, height, prevout.nValue, addrType.first, addrType.second)));
}

void handleRemint(CTxIn const & input, uint256 const & txHash, int height, int txNumber, CAmount nValue,
        AddressApollonPtr & addressApollon, AddressUnspentApollonPtr & addressUnspentApollon, SpentApollonPtr & spentApollon)
{
    if(!input.IsZerocoinRemint())
        return;

    if (addressApollon) {
        addressApollon->push_back(make_pair(CAddressApollonKey(AddressType::zerocoinRemint, uint160(), height, txNumber, txHash, 0, true), nValue * -1));
        addressUnspentApollon->push_back(make_pair(CAddressUnspentKey(AddressType::zerocoinRemint, uint160(), input.prevout.hash, input.prevout.n), CAddressUnspentValue()));
    }

    if (spentApollon)
        spentApollon->push_back(make_pair(CSpentApollonKey(input.prevout.hash, input.prevout.n), CSpentApollonValue(txHash, 0, height, nValue, AddressType::zerocoinRemint, uint160())));
}


template <class Iterator>
void handleZerocoinSpend(Iterator const begin, Iterator const end, uint256 const & txHash, int height, int txNumber, CCoinsViewCache const & view,
        AddressApollonPtr & addressApollon, bool isV3)
{
    if(!addressApollon)
        return;

    CAmount spendAmount = 0;
    for(Iterator iter = begin; iter != end; ++iter)
        spendAmount += iter->nValue;

    addressApollon->push_back(make_pair(CAddressApollonKey(isV3 ? AddressType::sigmaSpend : AddressType::zerocoinSpend, uint160(), height, txNumber, txHash, 0, true), -spendAmount));
}

void handleOutput(const CTxOut &out, size_t outNo, uint256 const & txHash, int height, int txNumber, CCoinsViewCache const & view, bool coinbase,
        AddressApollonPtr & addressApollon, AddressUnspentApollonPtr & addressUnspentApollon, SpentApollonPtr & spentApollon)
{
    if(!addressApollon)
        return;

    if(out.scriptPubKey.IsZerocoinMint())
        addressApollon->push_back(make_pair(CAddressApollonKey(AddressType::zerocoinMint, uint160(), height, txNumber, txHash, outNo, false), out.nValue));

    if(out.scriptPubKey.IsSigmaMint())
        addressApollon->push_back(make_pair(CAddressApollonKey(AddressType::sigmaMint, uint160(), height, txNumber, txHash, outNo, false), out.nValue));

    txnouttype type;
    vector<vector<unsigned char> > addresses;

    if(!Solver(out.scriptPubKey, type, addresses)) {
        LogPrint("CDbApollonHelper", "Encountered an unsoluble script in block:%i, txHash: %s, outNo: %i\n", height, txHash.ToString().c_str(), outNo);
        return;
    }

    std::pair<AddressType, uint160> addrType = classifyAddress(type, addresses);

    if(addrType.first == AddressType::unknown) {
        return;
    }

    addressApollon->push_back(make_pair(CAddressApollonKey(addrType.first, addrType.second, height, txNumber, txHash, outNo, false), out.nValue));
    addressUnspentApollon->push_back(make_pair(CAddressUnspentKey(addrType.first, addrType.second, txHash, outNo), CAddressUnspentValue(out.nValue, out.scriptPubKey, height)));
}
}


void CDbApollonHelper::ConnectTransaction(CTransaction const & tx, int height, int txNumber, CCoinsViewCache const & view)
{
    size_t no = 0;
    if(!tx.IsCoinBase() && !tx.IsZerocoinSpend() && !tx.IsSigmaSpend() && !tx.IsZerocoinRemint()) {
        for (CTxIn const & input : tx.vin) {
            handleInput(input, no++, tx.GetHash(), height, txNumber, view, addressApollon, addressUnspentApollon, spentApollon);
        }
    }

    if(tx.IsZerocoinRemint()) {
        CAmount remintValue = 0;
        for (CTxOut const & out : tx.vout) {
            remintValue += out.nValue;
        }
        if (tx.vin.size() != 1) {
           error("A Zerocoin to Sigma remint tx shoud have just 1 input");
           return;
        }
        handleRemint(tx.vin[0], tx.GetHash(), height, txNumber, remintValue, addressApollon, addressUnspentApollon, spentApollon);
    }

    if(tx.IsZerocoinSpend() || tx.IsSigmaSpend())
        handleZerocoinSpend(tx.vout.begin(), tx.vout.end(), tx.GetHash(), height, txNumber, view, addressApollon, tx.IsSigmaSpend());

    no = 0;
    bool const txIsCoinBase = tx.IsCoinBase();
    for (CTxOut const & out : tx.vout) {
        handleOutput(out, no++, tx.GetHash(), height, txNumber, view, txIsCoinBase, addressApollon, addressUnspentApollon, spentApollon);
    }
}


void CDbApollonHelper::DisconnectTransactionInputs(CTransaction const & tx, int height, int txNumber, CCoinsViewCache const & view)
{
    size_t pAddressBegin{0}, pUnspentBegin{0}, pSpentBegin{0};

    if(addressApollon){
        pAddressBegin = addressApollon->size();
        pUnspentBegin = addressUnspentApollon->size();
    }

    if(spentApollon)
        pSpentBegin = spentApollon->size();

    if(tx.IsZerocoinRemint()) {
        CAmount remintValue = 0;
        for (CTxOut const & out : tx.vout) {
            remintValue += out.nValue;
        }
        if (tx.vin.size() != 1) {
           error("A Zerocoin to Sigma remint tx shoud have just 1 input");
           return;
        }
        handleRemint(tx.vin[0], tx.GetHash(), height, txNumber, remintValue, addressApollon, addressUnspentApollon, spentApollon);
    }

    size_t no = 0;

    if(!tx.IsCoinBase() && !tx.IsZerocoinSpend() && !tx.IsSigmaSpend() && !tx.IsZerocoinRemint())
        for (CTxIn const & input : tx.vin) {
            handleInput(input, no++, tx.GetHash(), height, txNumber, view, addressApollon, addressUnspentApollon, spentApollon);
        }

    if(addressApollon){
        std::reverse(addressApollon->begin() + pAddressBegin, addressApollon->end());
        std::reverse(addressUnspentApollon->begin() + pUnspentBegin, addressUnspentApollon->end());

        for(AddressUnspentApollon::iterator iter = addressUnspentApollon->begin(); iter != addressUnspentApollon->end(); ++iter)
            iter->second = CAddressUnspentValue();
    }

    if(spentApollon)
        std::reverse(spentApollon->begin() + pSpentBegin, spentApollon->end());
}

void CDbApollonHelper::DisconnectTransactionOutputs(CTransaction const & tx, int height, int txNumber, CCoinsViewCache const & view)
{
    if(tx.IsZerocoinSpend() || tx.IsSigmaSpend())
        handleZerocoinSpend(tx.vout.begin(), tx.vout.end(), tx.GetHash(), height, txNumber, view, addressApollon, tx.IsSigmaSpend());

    size_t no = 0;
    bool const txIsCoinBase = tx.IsCoinBase();
    for (CTxOut const & out : tx.vout) {
        handleOutput(out, no++, tx.GetHash(), height, txNumber, view, txIsCoinBase, addressApollon, addressUnspentApollon, spentApollon);
    }

    if(addressApollon)
    {
        std::reverse(addressApollon->begin(), addressApollon->end());
        std::reverse(addressUnspentApollon->begin(), addressUnspentApollon->end());
    }

    if(spentApollon)
        std::reverse(spentApollon->begin(), spentApollon->end());
}

CDbApollonHelper::AddressApollon const & CDbApollonHelper::getAddressApollon() const
{
    return *addressApollon;
}


CDbApollonHelper::AddressUnspentApollon const & CDbApollonHelper::getAddressUnspentApollon() const
{
    return *addressUnspentApollon;
}


CDbApollonHelper::SpentApollon const & CDbApollonHelper::getSpentApollon() const
{
    return *spentApollon;
}
