// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_TXDB_H
#define BITCOIN_TXDB_H

#include "coins.h"
#include "dbwrapper.h"
#include "chain.h"
#include "spentapollon.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <boost/function.hpp>

class CBlockApollon;
class CCoinsViewDBCursor;
class uint256;

//! -dbcache default (MiB)
static const int64_t nDefaultDbCache = 300;
//! max. -dbcache (MiB)
static const int64_t nMaxDbCache = sizeof(void*) > 4 ? 16384 : 1024;
//! min. -dbcache (MiB)
static const int64_t nMinDbCache = 4;
//! Max memory allocated to block tree DB specific cache, if no -txapollon (MiB)
static const int64_t nMaxBlockDBCache = 2;
//! Max memory allocated to block tree DB specific cache, if -txapollon (MiB)
// Unlike for the UTXO database, for the txapollon scenario the leveldb cache make
// a meaningful difference: https://github.com/bitcoin/bitcoin/pull/8273#issuecomment-229601991
static const int64_t nMaxBlockDBAndTxApollonCache = 1024;
//! Max memory allocated to coin DB specific cache (MiB)
static const int64_t nMaxCoinsDBCache = 8;

struct CDiskTxPos : public CDiskBlockPos
{
    unsigned int nTxOffset; // after header

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(*(CDiskBlockPos*)this);
        READWRITE(VARINT(nTxOffset));
    }

    CDiskTxPos(const CDiskBlockPos &blockIn, unsigned int nTxOffsetIn) : CDiskBlockPos(blockIn.nFile, blockIn.nPos), nTxOffset(nTxOffsetIn) {
    }

    CDiskTxPos() {
        SetNull();
    }

    void SetNull() {
        CDiskBlockPos::SetNull();
        nTxOffset = 0;
    }
};

/** CCoinsView backed by the coin database (chainstate/) */
class CCoinsViewDB : public CCoinsView
{
protected:
    CDBWrapper db;
public:
    CCoinsViewDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

    bool GetCoins(const uint256 &txid, CCoins &coins) const;
    bool HaveCoins(const uint256 &txid) const;
    uint256 GetBestBlock() const;
    bool BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock);
    CCoinsViewCursor *Cursor() const;
};

/** Specialization of CCoinsViewCursor to iterate over a CCoinsViewDB */
class CCoinsViewDBCursor: public CCoinsViewCursor
{
public:
    ~CCoinsViewDBCursor() {}

    bool GetKey(uint256 &key) const;
    bool GetValue(CCoins &coins) const;
    unsigned int GetValueSize() const;

    bool Valid() const;
    void Next();

private:
    CCoinsViewDBCursor(CDBIterator* pcursorIn, const uint256 &hashBlockIn):
        CCoinsViewCursor(hashBlockIn), pcursor(pcursorIn) {}
    boost::scoped_ptr<CDBIterator> pcursor;
    std::pair<char, uint256> keyTmp;

    friend class CCoinsViewDB;
};

/** Access to the block database (blocks/apollon/) */
class CBlockTreeDB : public CDBWrapper
{
public:
    CBlockTreeDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);
private:
    CBlockTreeDB(const CBlockTreeDB&);
    void operator=(const CBlockTreeDB&);
public:
    bool WriteBatchSync(const std::vector<std::pair<int, const CBlockFileInfo*> >& fileInfo, int nLastFile, const std::vector<const CBlockApollon*>& blockinfo);
    bool ReadBlockFileInfo(int nFile, CBlockFileInfo &fileinfo);
    bool ReadLastBlockFile(int &nFile);
    bool WriteReapolloning(bool fReapollon);
    bool ReadReapolloning(bool &fReapollon);
    bool ReadTxApollon(const uint256 &txid, CDiskTxPos &pos);
    bool WriteTxApollon(const std::vector<std::pair<uint256, CDiskTxPos> > &list);
    bool ReadSpentApollon(CSpentApollonKey &key, CSpentApollonValue &value);
    bool UpdateSpentApollon(const std::vector<std::pair<CSpentApollonKey, CSpentApollonValue> >&vect);
    bool UpdateAddressUnspentApollon(const std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue > >&vect);
    bool ReadAddressUnspentApollon(uint160 addressHash, AddressType type,
                                 std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &vect);
    bool WriteAddressApollon(const std::vector<std::pair<CAddressApollonKey, CAmount> > &vect);
    bool EraseAddressApollon(const std::vector<std::pair<CAddressApollonKey, CAmount> > &vect);
    bool ReadAddressApollon(uint160 addressHash, AddressType type,
                          std::vector<std::pair<CAddressApollonKey, CAmount> > &addressApollon,
                          int start = 0, int end = 0);

    bool WriteTimestampApollon(const CTimestampApollonKey &timestampApollon);
    bool ReadTimestampApollon(const unsigned int &high, const unsigned int &low, std::vector<uint256> &vect);
    bool WriteFlag(const std::string &name, bool fValue);
    bool ReadFlag(const std::string &name, bool &fValue);
    bool LoadBlockApollonGuts(boost::function<CBlockApollon*(const uint256&)> insertBlockApollon);
    int GetBlockApollonVersion();
    int GetBlockApollonVersion(uint256 const & blockHash);
    bool AddTotalSupply(CAmount const & supply);
    bool ReadTotalSupply(CAmount & supply);
};


/**
 * This class was introduced as the logic for address and tx indices became too intricate.
 *
 * @param addressApollon, spentApollon - true if to update the corresponding apollon
 *
 * It is undefined behavior if the helper was created with addressApollon == false
 * and getAddressApollon was called later (same for spentApollon and unspentApollon).
 */
class CDbApollonHelper : boost::noncopyable
{
public:
    CDbApollonHelper(bool addressApollon, bool spentApollon);

    void ConnectTransaction(CTransaction const & tx, int height, int txNumber, CCoinsViewCache const & view);
    void DisconnectTransactionInputs(CTransaction const & tx, int height, int txNumber, CCoinsViewCache const & view);
    void DisconnectTransactionOutputs(CTransaction const & tx, int height, int txNumber, CCoinsViewCache const & view);

    using AddressApollon = std::vector<std::pair<CAddressApollonKey, CAmount> >;
    using AddressUnspentApollon = std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >;
    using SpentApollon = std::vector<std::pair<CSpentApollonKey, CSpentApollonValue> >;

    AddressApollon const & getAddressApollon() const;
    AddressUnspentApollon const & getAddressUnspentApollon() const;
    SpentApollon const & getSpentApollon() const;

private:
    boost::optional<AddressApollon> addressApollon;
    boost::optional<AddressUnspentApollon> addressUnspentApollon;
    boost::optional<SpentApollon> spentApollon;
};

#endif // BITCOIN_TXDB_H
