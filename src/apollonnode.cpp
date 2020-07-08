// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activeapollonnode.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "darksend.h"
#include "init.h"
//#include "governance.h"
#include "apollonnode.h"
#include "apollonnode-payments.h"
#include "apollonnodeconfig.h"
#include "apollonnode-sync.h"
#include "apollonnodeman.h"
#include "util.h"
#include "validationinterface.h"

#include <boost/lexical_cast.hpp>


CApollonnode::CApollonnode() :
        vin(),
        addr(),
        pubKeyCollateralAddress(),
        pubKeyApollonnode(),
        lastPing(),
        vchSig(),
        sigTime(GetAdjustedTime()),
        nLastDsq(0),
        nTimeLastChecked(0),
        nTimeLastPaid(0),
        nTimeLastWatchdogVote(0),
        nActiveState(APOLLONNODE_ENABLED),
        nCacheCollateralBlock(0),
        nBlockLastPaid(0),
        nProtocolVersion(PROTOCOL_VERSION),
        nPoSeBanScore(0),
        nPoSeBanHeight(0),
        fAllowMixingTx(true),
        fUnitTest(false) {}

CApollonnode::CApollonnode(CService addrNew, CTxIn vinNew, CPubKey pubKeyCollateralAddressNew, CPubKey pubKeyApollonnodeNew, int nProtocolVersionIn) :
        vin(vinNew),
        addr(addrNew),
        pubKeyCollateralAddress(pubKeyCollateralAddressNew),
        pubKeyApollonnode(pubKeyApollonnodeNew),
        lastPing(),
        vchSig(),
        sigTime(GetAdjustedTime()),
        nLastDsq(0),
        nTimeLastChecked(0),
        nTimeLastPaid(0),
        nTimeLastWatchdogVote(0),
        nActiveState(APOLLONNODE_ENABLED),
        nCacheCollateralBlock(0),
        nBlockLastPaid(0),
        nProtocolVersion(nProtocolVersionIn),
        nPoSeBanScore(0),
        nPoSeBanHeight(0),
        fAllowMixingTx(true),
        fUnitTest(false) {}

CApollonnode::CApollonnode(const CApollonnode &other) :
        vin(other.vin),
        addr(other.addr),
        pubKeyCollateralAddress(other.pubKeyCollateralAddress),
        pubKeyApollonnode(other.pubKeyApollonnode),
        lastPing(other.lastPing),
        vchSig(other.vchSig),
        sigTime(other.sigTime),
        nLastDsq(other.nLastDsq),
        nTimeLastChecked(other.nTimeLastChecked),
        nTimeLastPaid(other.nTimeLastPaid),
        nTimeLastWatchdogVote(other.nTimeLastWatchdogVote),
        nActiveState(other.nActiveState),
        nCacheCollateralBlock(other.nCacheCollateralBlock),
        nBlockLastPaid(other.nBlockLastPaid),
        nProtocolVersion(other.nProtocolVersion),
        nPoSeBanScore(other.nPoSeBanScore),
        nPoSeBanHeight(other.nPoSeBanHeight),
        fAllowMixingTx(other.fAllowMixingTx),
        fUnitTest(other.fUnitTest) {}

CApollonnode::CApollonnode(const CApollonnodeBroadcast &mnb) :
        vin(mnb.vin),
        addr(mnb.addr),
        pubKeyCollateralAddress(mnb.pubKeyCollateralAddress),
        pubKeyApollonnode(mnb.pubKeyApollonnode),
        lastPing(mnb.lastPing),
        vchSig(mnb.vchSig),
        sigTime(mnb.sigTime),
        nLastDsq(0),
        nTimeLastChecked(0),
        nTimeLastPaid(0),
        nTimeLastWatchdogVote(mnb.sigTime),
        nActiveState(mnb.nActiveState),
        nCacheCollateralBlock(0),
        nBlockLastPaid(0),
        nProtocolVersion(mnb.nProtocolVersion),
        nPoSeBanScore(0),
        nPoSeBanHeight(0),
        fAllowMixingTx(true),
        fUnitTest(false) {}

//CSporkManager sporkManager;
//
// When a new apollonnode broadcast is sent, update our information
//
bool CApollonnode::UpdateFromNewBroadcast(CApollonnodeBroadcast &mnb) {
    if (mnb.sigTime <= sigTime && !mnb.fRecovery) return false;

    pubKeyApollonnode = mnb.pubKeyApollonnode;
    sigTime = mnb.sigTime;
    vchSig = mnb.vchSig;
    nProtocolVersion = mnb.nProtocolVersion;
    addr = mnb.addr;
    nPoSeBanScore = 0;
    nPoSeBanHeight = 0;
    nTimeLastChecked = 0;
    int nDos = 0;
    if (mnb.lastPing == CApollonnodePing() || (mnb.lastPing != CApollonnodePing() && mnb.lastPing.CheckAndUpdate(this, true, nDos))) {
        SetLastPing(mnb.lastPing);
        mnodeman.mapSeenApollonnodePing.insert(std::make_pair(lastPing.GetHash(), lastPing));
    }
    // if it matches our Apollonnode privkey...
    if (fApollonNode && pubKeyApollonnode == activeApollonnode.pubKeyApollonnode) {
        nPoSeBanScore = -APOLLONNODE_POSE_BAN_MAX_SCORE;
        if (nProtocolVersion == PROTOCOL_VERSION) {
            // ... and PROTOCOL_VERSION, then we've been remotely activated ...
            activeApollonnode.ManageState();
        } else {
            // ... otherwise we need to reactivate our node, do not add it to the list and do not relay
            // but also do not ban the node we get this message from
            LogPrintf("CApollonnode::UpdateFromNewBroadcast -- wrong PROTOCOL_VERSION, re-activate your MN: message nProtocolVersion=%d  PROTOCOL_VERSION=%d\n", nProtocolVersion, PROTOCOL_VERSION);
            return false;
        }
    }
    return true;
}

//
// Deterministically calculate a given "score" for a Apollonnode depending on how close it's hash is to
// the proof of work for that block. The further away they are the better, the furthest will win the election
// and get paid this block
//
arith_uint256 CApollonnode::CalculateScore(const uint256 &blockHash) {
    uint256 aux = ArithToUint256(UintToArith256(vin.prevout.hash) + vin.prevout.n);

    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << blockHash;
    arith_uint256 hash2 = UintToArith256(ss.GetHash());

    CHashWriter ss2(SER_GETHASH, PROTOCOL_VERSION);
    ss2 << blockHash;
    ss2 << aux;
    arith_uint256 hash3 = UintToArith256(ss2.GetHash());

    return (hash3 > hash2 ? hash3 - hash2 : hash2 - hash3);
}

void CApollonnode::Check(bool fForce) {
    LOCK(cs);

    if (ShutdownRequested()) return;

    if (!fForce && (GetTime() - nTimeLastChecked < APOLLONNODE_CHECK_SECONDS)) return;
    nTimeLastChecked = GetTime();

    LogPrint("apollonnode", "CApollonnode::Check -- Apollonnode %s is in %s state\n", vin.prevout.ToStringShort(), GetStateString());

    //once spent, stop doing the checks
    if (IsOutpointSpent()) return;

    int nHeight = 0;
    if (!fUnitTest) {
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain) return;

        CCoins coins;
        if (!pcoinsTip->GetCoins(vin.prevout.hash, coins) ||
            (unsigned int) vin.prevout.n >= coins.vout.size() ||
            coins.vout[vin.prevout.n].IsNull()) {
            SetStatus(APOLLONNODE_OUTPOINT_SPENT);
            LogPrint("apollonnode", "CApollonnode::Check -- Failed to find Apollonnode UTXO, apollonnode=%s\n", vin.prevout.ToStringShort());
            return;
        }

        nHeight = chainActive.Height();
    }

    if (IsPoSeBanned()) {
        if (nHeight < nPoSeBanHeight) return; // too early?
        // Otherwise give it a chance to proceed further to do all the usual checks and to change its state.
        // Apollonnode still will be on the edge and can be banned back easily if it keeps ignoring mnverify
        // or connect attempts. Will require few mnverify messages to strengthen its position in mn list.
        LogPrintf("CApollonnode::Check -- Apollonnode %s is unbanned and back in list now\n", vin.prevout.ToStringShort());
        DecreasePoSeBanScore();
    } else if (nPoSeBanScore >= APOLLONNODE_POSE_BAN_MAX_SCORE) {
        SetStatus(APOLLONNODE_POSE_BAN);
        // ban for the whole payment cycle
        nPoSeBanHeight = nHeight + mnodeman.size();
        LogPrintf("CApollonnode::Check -- Apollonnode %s is banned till block %d now\n", vin.prevout.ToStringShort(), nPoSeBanHeight);
        return;
    }

    int nActiveStatePrev = nActiveState;
    bool fOurApollonnode = fApollonNode && activeApollonnode.pubKeyApollonnode == pubKeyApollonnode;

    // apollonnode doesn't meet payment protocol requirements ...
/*    bool fRequireUpdate = nProtocolVersion < mnpayments.GetMinApollonnodePaymentsProto() ||
                          // or it's our own node and we just updated it to the new protocol but we are still waiting for activation ...
                          (fOurApollonnode && nProtocolVersion < PROTOCOL_VERSION); */

    // apollonnode doesn't meet payment protocol requirements ...
    bool fRequireUpdate = nProtocolVersion < mnpayments.GetMinApollonnodePaymentsProto() ||
                          // or it's our own node and we just updated it to the new protocol but we are still waiting for activation ...
                          (fOurApollonnode && (nProtocolVersion < MIN_APOLLONNODE_PAYMENT_PROTO_VERSION_1 || nProtocolVersion > MIN_APOLLONNODE_PAYMENT_PROTO_VERSION_2));

    if (fRequireUpdate) {
        SetStatus(APOLLONNODE_UPDATE_REQUIRED);
        if (nActiveStatePrev != nActiveState) {
            LogPrint("apollonnode", "CApollonnode::Check -- Apollonnode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
        }
        return;
    }

    // keep old apollonnodes on start, give them a chance to receive updates...
    bool fWaitForPing = !apollonnodeSync.IsApollonnodeListSynced() && !IsPingedWithin(APOLLONNODE_MIN_MNP_SECONDS);

    if (fWaitForPing && !fOurApollonnode) {
        // ...but if it was already expired before the initial check - return right away
        if (IsExpired() || IsWatchdogExpired() || IsNewStartRequired()) {
            LogPrint("apollonnode", "CApollonnode::Check -- Apollonnode %s is in %s state, waiting for ping\n", vin.prevout.ToStringShort(), GetStateString());
            return;
        }
    }

    // don't expire if we are still in "waiting for ping" mode unless it's our own apollonnode
    if (!fWaitForPing || fOurApollonnode) {

        if (!IsPingedWithin(APOLLONNODE_NEW_START_REQUIRED_SECONDS)) {
            SetStatus(APOLLONNODE_NEW_START_REQUIRED);
            if (nActiveStatePrev != nActiveState) {
                LogPrint("apollonnode", "CApollonnode::Check -- Apollonnode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
            }
            return;
        }

        bool fWatchdogActive = apollonnodeSync.IsSynced() && mnodeman.IsWatchdogActive();
        bool fWatchdogExpired = (fWatchdogActive && ((GetTime() - nTimeLastWatchdogVote) > APOLLONNODE_WATCHDOG_MAX_SECONDS));

//        LogPrint("apollonnode", "CApollonnode::Check -- outpoint=%s, nTimeLastWatchdogVote=%d, GetTime()=%d, fWatchdogExpired=%d\n",
//                vin.prevout.ToStringShort(), nTimeLastWatchdogVote, GetTime(), fWatchdogExpired);

        if (fWatchdogExpired) {
            SetStatus(APOLLONNODE_WATCHDOG_EXPIRED);
            if (nActiveStatePrev != nActiveState) {
                LogPrint("apollonnode", "CApollonnode::Check -- Apollonnode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
            }
            return;
        }

        if (!IsPingedWithin(APOLLONNODE_EXPIRATION_SECONDS)) {
            SetStatus(APOLLONNODE_EXPIRED);
            if (nActiveStatePrev != nActiveState) {
                LogPrint("apollonnode", "CApollonnode::Check -- Apollonnode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
            }
            return;
        }
    }

    if (lastPing.sigTime - sigTime < APOLLONNODE_MIN_MNP_SECONDS) {
        SetStatus(APOLLONNODE_PRE_ENABLED);
        if (nActiveStatePrev != nActiveState) {
            LogPrint("apollonnode", "CApollonnode::Check -- Apollonnode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
        }
        return;
    }

    SetStatus(APOLLONNODE_ENABLED); // OK
    if (nActiveStatePrev != nActiveState) {
        LogPrint("apollonnode", "CApollonnode::Check -- Apollonnode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
    }
}

bool CApollonnode::IsValidNetAddr() {
    return IsValidNetAddr(addr);
}

bool CApollonnode::IsValidForPayment() {
    if (nActiveState == APOLLONNODE_ENABLED) {
        return true;
    }
//    if(!sporkManager.IsSporkActive(SPORK_14_REQUIRE_SENTINEL_FLAG) &&
//       (nActiveState == APOLLONNODE_WATCHDOG_EXPIRED)) {
//        return true;
//    }

    return false;
}

bool CApollonnode::IsValidNetAddr(CService addrIn) {
    // TODO: regtest is fine with any addresses for now,
    // should probably be a bit smarter if one day we start to implement tests for this
    return Params().NetworkIDString() == CBaseChainParams::REGTEST ||
           (addrIn.IsIPv4() && IsReachable(addrIn) && addrIn.IsRoutable());
}

bool CApollonnode::IsMyApollonnode(){
    BOOST_FOREACH(CApollonnodeConfig::CApollonnodeEntry mne, apollonnodeConfig.getEntries()) {
        const std::string& txHash = mne.getTxHash();
        const std::string& outputApollon = mne.getOutputApollon();

        if(txHash==vin.prevout.hash.ToString().substr(0,64) &&
           outputApollon==to_string(vin.prevout.n))
            return true;
    }
    return false;
}

apollonnode_info_t CApollonnode::GetInfo() {
    apollonnode_info_t info;
    info.vin = vin;
    info.addr = addr;
    info.pubKeyCollateralAddress = pubKeyCollateralAddress;
    info.pubKeyApollonnode = pubKeyApollonnode;
    info.sigTime = sigTime;
    info.nLastDsq = nLastDsq;
    info.nTimeLastChecked = nTimeLastChecked;
    info.nTimeLastPaid = nTimeLastPaid;
    info.nTimeLastWatchdogVote = nTimeLastWatchdogVote;
    info.nTimeLastPing = lastPing.sigTime;
    info.nActiveState = nActiveState;
    info.nProtocolVersion = nProtocolVersion;
    info.fInfoValid = true;
    return info;
}

std::string CApollonnode::StateToString(int nStateIn) {
    switch (nStateIn) {
        case APOLLONNODE_PRE_ENABLED:
            return "PRE_ENABLED";
        case APOLLONNODE_ENABLED:
            return "ENABLED";
        case APOLLONNODE_EXPIRED:
            return "EXPIRED";
        case APOLLONNODE_OUTPOINT_SPENT:
            return "OUTPOINT_SPENT";
        case APOLLONNODE_UPDATE_REQUIRED:
            return "UPDATE_REQUIRED";
        case APOLLONNODE_WATCHDOG_EXPIRED:
            return "WATCHDOG_EXPIRED";
        case APOLLONNODE_NEW_START_REQUIRED:
            return "NEW_START_REQUIRED";
        case APOLLONNODE_POSE_BAN:
            return "POSE_BAN";
        default:
            return "UNKNOWN";
    }
}

std::string CApollonnode::GetStateString() const {
    return StateToString(nActiveState);
}

std::string CApollonnode::GetStatus() const {
    // TODO: return smth a bit more human readable here
    return GetStateString();
}

void CApollonnode::SetStatus(int newState) {
    if(nActiveState!=newState){
        nActiveState = newState;
        if(IsMyApollonnode())
            GetMainSignals().UpdatedApollonnode(*this);
    }
}

void CApollonnode::SetLastPing(CApollonnodePing newApollonnodePing) {
    if(lastPing!=newApollonnodePing){
        lastPing = newApollonnodePing;
        if(IsMyApollonnode())
            GetMainSignals().UpdatedApollonnode(*this);
    }
}

void CApollonnode::SetTimeLastPaid(int64_t newTimeLastPaid) {
     if(nTimeLastPaid!=newTimeLastPaid){
        nTimeLastPaid = newTimeLastPaid;
        if(IsMyApollonnode())
            GetMainSignals().UpdatedApollonnode(*this);
    }   
}

void CApollonnode::SetBlockLastPaid(int newBlockLastPaid) {
     if(nBlockLastPaid!=newBlockLastPaid){
        nBlockLastPaid = newBlockLastPaid;
        if(IsMyApollonnode())
            GetMainSignals().UpdatedApollonnode(*this);
    }   
}

void CApollonnode::SetRank(int newRank) {
     if(nRank!=newRank){
        nRank = newRank;
        if(nRank < 0 || nRank > mnodeman.size()) nRank = 0;
        if(IsMyApollonnode())
            GetMainSignals().UpdatedApollonnode(*this);
    }   
}

std::string CApollonnode::ToString() const {
    std::string str;
    str += "apollonnode{";
    str += addr.ToString();
    str += " ";
    str += std::to_string(nProtocolVersion);
    str += " ";
    str += vin.prevout.ToStringShort();
    str += " ";
    str += CBitcoinAddress(pubKeyCollateralAddress.GetID()).ToString();
    str += " ";
    str += std::to_string(lastPing == CApollonnodePing() ? sigTime : lastPing.sigTime);
    str += " ";
    str += std::to_string(lastPing == CApollonnodePing() ? 0 : lastPing.sigTime - sigTime);
    str += " ";
    str += std::to_string(nBlockLastPaid);
    str += "}\n";
    return str;
}

UniValue CApollonnode::ToJSON() const {
    UniValue ret(UniValue::VOBJ);
    std::string payee = CBitcoinAddress(pubKeyCollateralAddress.GetID()).ToString();
    COutPoint outpoint = vin.prevout;
    UniValue outpointObj(UniValue::VOBJ);
    UniValue authorityObj(UniValue::VOBJ);
    outpointObj.push_back(Pair("txid", outpoint.hash.ToString().substr(0,64)));
    outpointObj.push_back(Pair("apollon", to_string(outpoint.n)));

    std::string authority = addr.ToString();
    std::string ip   = authority.substr(0, authority.find(":"));
    std::string port = authority.substr(authority.find(":")+1, authority.length());
    authorityObj.push_back(Pair("ip", ip));
    authorityObj.push_back(Pair("port", port));
    
    // get myApollonnode data
    bool isMine = false;
    string label;
    int fApollon=0;
    BOOST_FOREACH(CApollonnodeConfig::CApollonnodeEntry mne, apollonnodeConfig.getEntries()) {
        CTxIn myVin = CTxIn(uint256S(mne.getTxHash()), uint32_t(atoi(mne.getOutputApollon().c_str())));
        if(outpoint.ToStringShort()==myVin.prevout.ToStringShort()){
            isMine = true;
            label = mne.getAlias();
            break;
        }
        fApollon++;
    }

    ret.push_back(Pair("rank", nRank));
    ret.push_back(Pair("outpoint", outpointObj));
    ret.push_back(Pair("status", GetStatus()));
    ret.push_back(Pair("protocolVersion", nProtocolVersion));
    ret.push_back(Pair("payeeAddress", payee));
    ret.push_back(Pair("lastSeen", (int64_t) lastPing.sigTime * 1000));
    ret.push_back(Pair("activeSince", (int64_t)(sigTime * 1000)));
    ret.push_back(Pair("lastPaidTime", (int64_t) GetLastPaidTime() * 1000));
    ret.push_back(Pair("lastPaidBlock", GetLastPaidBlock()));
    ret.push_back(Pair("authority", authorityObj));
    ret.push_back(Pair("isMine", isMine));
    if(isMine){
        ret.push_back(Pair("label", label));
        ret.push_back(Pair("position", fApollon));
    }

    UniValue qualify(UniValue::VOBJ);

    CApollonnode* apollonnode = const_cast <CApollonnode*> (this);
    qualify = mnodeman.GetNotQualifyReasonToUniValue(*apollonnode, chainActive.Tip()->nHeight, true, mnodeman.CountEnabled());
    ret.push_back(Pair("qualify", qualify));

    return ret;
}

int CApollonnode::GetCollateralAge() {
    int nHeight;
    {
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain || !chainActive.Tip()) return -1;
        nHeight = chainActive.Height();
    }

    if (nCacheCollateralBlock == 0) {
        int nInputAge = GetInputAge(vin);
        if (nInputAge > 0) {
            nCacheCollateralBlock = nHeight - nInputAge;
        } else {
            return nInputAge;
        }
    }

    return nHeight - nCacheCollateralBlock;
}

void CApollonnode::UpdateLastPaid(const CBlockApollon *papollon, int nMaxBlocksToScanBack) {
    if (!papollon) {
        LogPrintf("CApollonnode::UpdateLastPaid papollon is NULL\n");
        return;
    }

    const Consensus::Params &params = Params().GetConsensus();
    const CBlockApollon *BlockReading = papollon;

    CScript mnpayee = GetScriptForDestination(pubKeyCollateralAddress.GetID());
    LogPrint("apollonnode", "CApollonnode::UpdateLastPaidBlock -- searching for block with payment to %s\n", vin.prevout.ToStringShort());

    LOCK(cs_mapApollonnodeBlocks);

    for (int i = 0; BlockReading && BlockReading->nHeight > nBlockLastPaid && i < nMaxBlocksToScanBack; i++) {
//        LogPrintf("mnpayments.mapApollonnodeBlocks.count(BlockReading->nHeight)=%s\n", mnpayments.mapApollonnodeBlocks.count(BlockReading->nHeight));
//        LogPrintf("mnpayments.mapApollonnodeBlocks[BlockReading->nHeight].HasPayeeWithVotes(mnpayee, 2)=%s\n", mnpayments.mapApollonnodeBlocks[BlockReading->nHeight].HasPayeeWithVotes(mnpayee, 2));
        if (mnpayments.mapApollonnodeBlocks.count(BlockReading->nHeight) &&
            mnpayments.mapApollonnodeBlocks[BlockReading->nHeight].HasPayeeWithVotes(mnpayee, 2)) {
            // LogPrintf("i=%s, BlockReading->nHeight=%s\n", i, BlockReading->nHeight);
            CBlock block;
            if (!ReadBlockFromDisk(block, BlockReading, Params().GetConsensus())) // shouldn't really happen
            {
                LogPrintf("ReadBlockFromDisk failed\n");
                continue;
            }
            CAmount nApollonnodePayment = GetApollonnodePayment(params, false,BlockReading->nHeight);

            BOOST_FOREACH(CTxOut txout, block.vtx[0].vout)
            if (mnpayee == txout.scriptPubKey && nApollonnodePayment == txout.nValue) {
                SetBlockLastPaid(BlockReading->nHeight);
                SetTimeLastPaid(BlockReading->nTime);
                LogPrint("apollonnode", "CApollonnode::UpdateLastPaidBlock -- searching for block with payment to %s -- found new %d\n", vin.prevout.ToStringShort(), nBlockLastPaid);
                return;
            }
        }

        if (BlockReading->pprev == NULL) {
            assert(BlockReading);
            break;
        }
        BlockReading = BlockReading->pprev;
    }

    // Last payment for this apollonnode wasn't found in latest mnpayments blocks
    // or it was found in mnpayments blocks but wasn't found in the blockchain.
    // LogPrint("apollonnode", "CApollonnode::UpdateLastPaidBlock -- searching for block with payment to %s -- keeping old %d\n", vin.prevout.ToStringShort(), nBlockLastPaid);
}

bool CApollonnodeBroadcast::Create(std::string strService, std::string strKeyApollonnode, std::string strTxHash, std::string strOutputApollon, std::string &strErrorRet, CApollonnodeBroadcast &mnbRet, bool fOffline) {
    LogPrintf("CApollonnodeBroadcast::Create\n");
    CTxIn txin;
    CPubKey pubKeyCollateralAddressNew;
    CKey keyCollateralAddressNew;
    CPubKey pubKeyApollonnodeNew;
    CKey keyApollonnodeNew;
    //need correct blocks to send ping
    if (!fOffline && !apollonnodeSync.IsBlockchainSynced()) {
        strErrorRet = "Sync in progress. Must wait until sync is complete to start Apollonnode";
        LogPrintf("CApollonnodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    //TODO
    if (!darkSendSigner.GetKeysFromSecret(strKeyApollonnode, keyApollonnodeNew, pubKeyApollonnodeNew)) {
        strErrorRet = strprintf("Invalid apollonnode key %s", strKeyApollonnode);
        LogPrintf("CApollonnodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    if (!pwalletMain->GetApollonnodeVinAndKeys(txin, pubKeyCollateralAddressNew, keyCollateralAddressNew, strTxHash, strOutputApollon)) {
        strErrorRet = strprintf("Could not allocate txin %s:%s for apollonnode %s", strTxHash, strOutputApollon, strService);
        LogPrintf("CApollonnodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    CService service = CService(strService);
    int mainnetDefaultPort = Params(CBaseChainParams::MAIN).GetDefaultPort();
    if (Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if (service.GetPort() != mainnetDefaultPort) {
            strErrorRet = strprintf("Invalid port %u for apollonnode %s, only %d is supported on mainnet.", service.GetPort(), strService, mainnetDefaultPort);
            LogPrintf("CApollonnodeBroadcast::Create -- %s\n", strErrorRet);
            return false;
        }
    } else if (service.GetPort() == mainnetDefaultPort) {
        strErrorRet = strprintf("Invalid port %u for apollonnode %s, %d is the only supported on mainnet.", service.GetPort(), strService, mainnetDefaultPort);
        LogPrintf("CApollonnodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    return Create(txin, CService(strService), keyCollateralAddressNew, pubKeyCollateralAddressNew, keyApollonnodeNew, pubKeyApollonnodeNew, strErrorRet, mnbRet);
}

bool CApollonnodeBroadcast::Create(CTxIn txin, CService service, CKey keyCollateralAddressNew, CPubKey pubKeyCollateralAddressNew, CKey keyApollonnodeNew, CPubKey pubKeyApollonnodeNew, std::string &strErrorRet, CApollonnodeBroadcast &mnbRet) {
    // wait for reapollon and/or import to finish
    if (fImporting || fReapollon) return false;

    LogPrint("apollonnode", "CApollonnodeBroadcast::Create -- pubKeyCollateralAddressNew = %s, pubKeyApollonnodeNew.GetID() = %s\n",
             CBitcoinAddress(pubKeyCollateralAddressNew.GetID()).ToString(),
             pubKeyApollonnodeNew.GetID().ToString());


    CApollonnodePing mnp(txin);
    if (!mnp.Sign(keyApollonnodeNew, pubKeyApollonnodeNew)) {
        strErrorRet = strprintf("Failed to sign ping, apollonnode=%s", txin.prevout.ToStringShort());
        LogPrintf("CApollonnodeBroadcast::Create -- %s\n", strErrorRet);
        mnbRet = CApollonnodeBroadcast();
        return false;
    }

    int nHeight = chainActive.Height();
    if (nHeight < ZC_MODULUS_V2_START_BLOCK) {
        mnbRet = CApollonnodeBroadcast(service, txin, pubKeyCollateralAddressNew, pubKeyApollonnodeNew, MIN_PEER_PROTO_VERSION);
    } else {
        mnbRet = CApollonnodeBroadcast(service, txin, pubKeyCollateralAddressNew, pubKeyApollonnodeNew, PROTOCOL_VERSION);
    }

    if (!mnbRet.IsValidNetAddr()) {
        strErrorRet = strprintf("Invalid IP address, apollonnode=%s", txin.prevout.ToStringShort());
        LogPrintf("CApollonnodeBroadcast::Create -- %s\n", strErrorRet);
        mnbRet = CApollonnodeBroadcast();
        return false;
    }
    mnbRet.SetLastPing(mnp);
    if (!mnbRet.Sign(keyCollateralAddressNew)) {
        strErrorRet = strprintf("Failed to sign broadcast, apollonnode=%s", txin.prevout.ToStringShort());
        LogPrintf("CApollonnodeBroadcast::Create -- %s\n", strErrorRet);
        mnbRet = CApollonnodeBroadcast();
        return false;
    }

    return true;
}

bool CApollonnodeBroadcast::SimpleCheck(int &nDos) {
    nDos = 0;

    // make sure addr is valid
    if (!IsValidNetAddr()) {
        LogPrintf("CApollonnodeBroadcast::SimpleCheck -- Invalid addr, rejected: apollonnode=%s  addr=%s\n",
                  vin.prevout.ToStringShort(), addr.ToString());
        return false;
    }

    // make sure signature isn't in the future (past is OK)
    if (sigTime > GetAdjustedTime() + 60 * 60) {
        LogPrintf("CApollonnodeBroadcast::SimpleCheck -- Signature rejected, too far into the future: apollonnode=%s\n", vin.prevout.ToStringShort());
        nDos = 1;
        return false;
    }

    // empty ping or incorrect sigTime/unknown blockhash
    if (lastPing == CApollonnodePing() || !lastPing.SimpleCheck(nDos)) {
        // one of us is probably forked or smth, just mark it as expired and check the rest of the rules
        SetStatus(APOLLONNODE_EXPIRED);
    }

    if (nProtocolVersion < mnpayments.GetMinApollonnodePaymentsProto()) {
        LogPrintf("CApollonnodeBroadcast::SimpleCheck -- ignoring outdated Apollonnode: apollonnode=%s  nProtocolVersion=%d\n", vin.prevout.ToStringShort(), nProtocolVersion);
        return false;
    }

    CScript pubkeyScript;
    pubkeyScript = GetScriptForDestination(pubKeyCollateralAddress.GetID());

    if (pubkeyScript.size() != 25) {
        LogPrintf("CApollonnodeBroadcast::SimpleCheck -- pubKeyCollateralAddress has the wrong size\n");
        nDos = 100;
        return false;
    }

    CScript pubkeyScript2;
    pubkeyScript2 = GetScriptForDestination(pubKeyApollonnode.GetID());

    if (pubkeyScript2.size() != 25) {
        LogPrintf("CApollonnodeBroadcast::SimpleCheck -- pubKeyApollonnode has the wrong size\n");
        nDos = 100;
        return false;
    }

    if (!vin.scriptSig.empty()) {
        LogPrintf("CApollonnodeBroadcast::SimpleCheck -- Ignore Not Empty ScriptSig %s\n", vin.ToString());
        nDos = 100;
        return false;
    }

    int mainnetDefaultPort = Params(CBaseChainParams::MAIN).GetDefaultPort();
    if (Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if (addr.GetPort() != mainnetDefaultPort) return false;
    } else if (addr.GetPort() == mainnetDefaultPort) return false;

    return true;
}

bool CApollonnodeBroadcast::Update(CApollonnode *pmn, int &nDos) {
    nDos = 0;

    if (pmn->sigTime == sigTime && !fRecovery) {
        // mapSeenApollonnodeBroadcast in CApollonnodeMan::CheckMnbAndUpdateApollonnodeList should filter legit duplicates
        // but this still can happen if we just started, which is ok, just do nothing here.
        return false;
    }

    // this broadcast is older than the one that we already have - it's bad and should never happen
    // unless someone is doing something fishy
    if (pmn->sigTime > sigTime) {
        LogPrintf("CApollonnodeBroadcast::Update -- Bad sigTime %d (existing broadcast is at %d) for Apollonnode %s %s\n",
                  sigTime, pmn->sigTime, vin.prevout.ToStringShort(), addr.ToString());
        return false;
    }

    pmn->Check();

    // apollonnode is banned by PoSe
    if (pmn->IsPoSeBanned()) {
        LogPrintf("CApollonnodeBroadcast::Update -- Banned by PoSe, apollonnode=%s\n", vin.prevout.ToStringShort());
        return false;
    }

    // IsVnAssociatedWithPubkey is validated once in CheckOutpoint, after that they just need to match
    if (pmn->pubKeyCollateralAddress != pubKeyCollateralAddress) {
        LogPrintf("CApollonnodeBroadcast::Update -- Got mismatched pubKeyCollateralAddress and vin\n");
        nDos = 33;
        return false;
    }

    if (!CheckSignature(nDos)) {
        LogPrintf("CApollonnodeBroadcast::Update -- CheckSignature() failed, apollonnode=%s\n", vin.prevout.ToStringShort());
        return false;
    }

    // if ther was no apollonnode broadcast recently or if it matches our Apollonnode privkey...
    if (!pmn->IsBroadcastedWithin(APOLLONNODE_MIN_MNB_SECONDS) || (fApollonNode && pubKeyApollonnode == activeApollonnode.pubKeyApollonnode)) {
        // take the newest entry
        LogPrintf("CApollonnodeBroadcast::Update -- Got UPDATED Apollonnode entry: addr=%s\n", addr.ToString());
        if (pmn->UpdateFromNewBroadcast((*this))) {
            pmn->Check();
            RelayApollonNode();
        }
        apollonnodeSync.AddedApollonnodeList();
        GetMainSignals().UpdatedApollonnode(*pmn);
    }

    return true;
}

bool CApollonnodeBroadcast::CheckOutpoint(int &nDos) {
    // we are a apollonnode with the same vin (i.e. already activated) and this mnb is ours (matches our Apollonnode privkey)
    // so nothing to do here for us
    if (fApollonNode && vin.prevout == activeApollonnode.vin.prevout && pubKeyApollonnode == activeApollonnode.pubKeyApollonnode) {
        return false;
    }

    if (!CheckSignature(nDos)) {
        LogPrintf("CApollonnodeBroadcast::CheckOutpoint -- CheckSignature() failed, apollonnode=%s\n", vin.prevout.ToStringShort());
        return false;
    }

    {
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain) {
            // not mnb fault, let it to be checked again later
            LogPrint("apollonnode", "CApollonnodeBroadcast::CheckOutpoint -- Failed to aquire lock, addr=%s", addr.ToString());
            mnodeman.mapSeenApollonnodeBroadcast.erase(GetHash());
            return false;
        }

        CCoins coins;
        if (!pcoinsTip->GetCoins(vin.prevout.hash, coins) ||
            (unsigned int) vin.prevout.n >= coins.vout.size() ||
            coins.vout[vin.prevout.n].IsNull()) {
            LogPrint("apollonnode", "CApollonnodeBroadcast::CheckOutpoint -- Failed to find Apollonnode UTXO, apollonnode=%s\n", vin.prevout.ToStringShort());
            return false;
        }
        if (coins.vout[vin.prevout.n].nValue != APOLLONNODE_COIN_REQUIRED * COIN) {
            LogPrint("apollonnode", "CApollonnodeBroadcast::CheckOutpoint -- Apollonnode UTXO should have 1000 XAP, apollonnode=%s\n", vin.prevout.ToStringShort());
            return false;
        }
        if (chainActive.Height() - coins.nHeight + 1 < Params().GetConsensus().nApollonnodeMinimumConfirmations) {
            LogPrintf("CApollonnodeBroadcast::CheckOutpoint -- Apollonnode UTXO must have at least %d confirmations, apollonnode=%s\n",
                      Params().GetConsensus().nApollonnodeMinimumConfirmations, vin.prevout.ToStringShort());
            // maybe we miss few blocks, let this mnb to be checked again later
            mnodeman.mapSeenApollonnodeBroadcast.erase(GetHash());
            return false;
        }
    }

    LogPrint("apollonnode", "CApollonnodeBroadcast::CheckOutpoint -- Apollonnode UTXO verified\n");

    // make sure the vout that was signed is related to the transaction that spawned the Apollonnode
    //  - this is expensive, so it's only done once per Apollonnode
    if (!darkSendSigner.IsVinAssociatedWithPubkey(vin, pubKeyCollateralAddress)) {
        LogPrintf("CApollonnodeMan::CheckOutpoint -- Got mismatched pubKeyCollateralAddress and vin\n");
        nDos = 33;
        return false;
    }

    // verify that sig time is legit in past
    // should be at least not earlier than block when 1000 XAP tx got nApollonnodeMinimumConfirmations
    uint256 hashBlock = uint256();
    CTransaction tx2;
    GetTransaction(vin.prevout.hash, tx2, Params().GetConsensus(), hashBlock, true);
    {
        LOCK(cs_main);
        BlockMap::iterator mi = mapBlockApollon.find(hashBlock);
        if (mi != mapBlockApollon.end() && (*mi).second) {
            CBlockApollon *pMNApollon = (*mi).second; // block for 1000 XAP tx -> 1 confirmation
            CBlockApollon *pConfApollon = chainActive[pMNApollon->nHeight + Params().GetConsensus().nApollonnodeMinimumConfirmations - 1]; // block where tx got nApollonnodeMinimumConfirmations
            if (pConfApollon->GetBlockTime() > sigTime) {
                LogPrintf("CApollonnodeBroadcast::CheckOutpoint -- Bad sigTime %d (%d conf block is at %d) for Apollonnode %s %s\n",
                          sigTime, Params().GetConsensus().nApollonnodeMinimumConfirmations, pConfApollon->GetBlockTime(), vin.prevout.ToStringShort(), addr.ToString());
                return false;
            }
        }
    }

    return true;
}

bool CApollonnodeBroadcast::Sign(CKey &keyCollateralAddress) {
    std::string strError;
    std::string strMessage;

    sigTime = GetAdjustedTime();

    strMessage = addr.ToString() + boost::lexical_cast<std::string>(sigTime) +
                 pubKeyCollateralAddress.GetID().ToString() + pubKeyApollonnode.GetID().ToString() +
                 boost::lexical_cast<std::string>(nProtocolVersion);

    if (!darkSendSigner.SignMessage(strMessage, vchSig, keyCollateralAddress)) {
        LogPrintf("CApollonnodeBroadcast::Sign -- SignMessage() failed\n");
        return false;
    }

    if (!darkSendSigner.VerifyMessage(pubKeyCollateralAddress, vchSig, strMessage, strError)) {
        LogPrintf("CApollonnodeBroadcast::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CApollonnodeBroadcast::CheckSignature(int &nDos) {
    std::string strMessage;
    std::string strError = "";
    nDos = 0;

    strMessage = addr.ToString() + boost::lexical_cast<std::string>(sigTime) +
                 pubKeyCollateralAddress.GetID().ToString() + pubKeyApollonnode.GetID().ToString() +
                 boost::lexical_cast<std::string>(nProtocolVersion);

    LogPrint("apollonnode", "CApollonnodeBroadcast::CheckSignature -- strMessage: %s  pubKeyCollateralAddress address: %s  sig: %s\n", strMessage, CBitcoinAddress(pubKeyCollateralAddress.GetID()).ToString(), EncodeBase64(&vchSig[0], vchSig.size()));

    if (!darkSendSigner.VerifyMessage(pubKeyCollateralAddress, vchSig, strMessage, strError)) {
        LogPrintf("CApollonnodeBroadcast::CheckSignature -- Got bad Apollonnode announce signature, error: %s\n", strError);
        nDos = 100;
        return false;
    }

    return true;
}

void CApollonnodeBroadcast::RelayApollonNode() {
    LogPrintf("CApollonnodeBroadcast::RelayApollonNode\n");
    CInv inv(MSG_APOLLONNODE_ANNOUNCE, GetHash());
    RelayInv(inv);
}

CApollonnodePing::CApollonnodePing(CTxIn &vinNew) {
    LOCK(cs_main);
    if (!chainActive.Tip() || chainActive.Height() < 12) return;

    vin = vinNew;
    blockHash = chainActive[chainActive.Height() - 12]->GetBlockHash();
    sigTime = GetAdjustedTime();
    vchSig = std::vector < unsigned char > ();
}

bool CApollonnodePing::Sign(CKey &keyApollonnode, CPubKey &pubKeyApollonnode) {
    std::string strError;
    std::string strApollonNodeSignMessage;

    sigTime = GetAdjustedTime();
    std::string strMessage = vin.ToString() + blockHash.ToString() + boost::lexical_cast<std::string>(sigTime);

    if (!darkSendSigner.SignMessage(strMessage, vchSig, keyApollonnode)) {
        LogPrintf("CApollonnodePing::Sign -- SignMessage() failed\n");
        return false;
    }

    if (!darkSendSigner.VerifyMessage(pubKeyApollonnode, vchSig, strMessage, strError)) {
        LogPrintf("CApollonnodePing::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CApollonnodePing::CheckSignature(CPubKey &pubKeyApollonnode, int &nDos) {
    std::string strMessage = vin.ToString() + blockHash.ToString() + boost::lexical_cast<std::string>(sigTime);
    std::string strError = "";
    nDos = 0;

    if (!darkSendSigner.VerifyMessage(pubKeyApollonnode, vchSig, strMessage, strError)) {
        LogPrintf("CApollonnodePing::CheckSignature -- Got bad Apollonnode ping signature, apollonnode=%s, error: %s\n", vin.prevout.ToStringShort(), strError);
        nDos = 33;
        return false;
    }
    return true;
}

bool CApollonnodePing::SimpleCheck(int &nDos) {
    // don't ban by default
    nDos = 0;

    if (sigTime > GetAdjustedTime() + 60 * 60) {
        LogPrintf("CApollonnodePing::SimpleCheck -- Signature rejected, too far into the future, apollonnode=%s\n", vin.prevout.ToStringShort());
        nDos = 1;
        return false;
    }

    {
//        LOCK(cs_main);
        AssertLockHeld(cs_main);
        BlockMap::iterator mi = mapBlockApollon.find(blockHash);
        if (mi == mapBlockApollon.end()) {
            LogPrint("apollonnode", "CApollonnodePing::SimpleCheck -- Apollonnode ping is invalid, unknown block hash: apollonnode=%s blockHash=%s\n", vin.prevout.ToStringShort(), blockHash.ToString());
            // maybe we stuck or forked so we shouldn't ban this node, just fail to accept this ping
            // TODO: or should we also request this block?
            return false;
        }
    }
    LogPrint("apollonnode", "CApollonnodePing::SimpleCheck -- Apollonnode ping verified: apollonnode=%s  blockHash=%s  sigTime=%d\n", vin.prevout.ToStringShort(), blockHash.ToString(), sigTime);
    return true;
}

bool CApollonnodePing::CheckAndUpdate(CApollonnode *pmn, bool fFromNewBroadcast, int &nDos) {
    // don't ban by default
    nDos = 0;

    if (!SimpleCheck(nDos)) {
        return false;
    }

    if (pmn == NULL) {
        LogPrint("apollonnode", "CApollonnodePing::CheckAndUpdate -- Couldn't find Apollonnode entry, apollonnode=%s\n", vin.prevout.ToStringShort());
        return false;
    }

    if (!fFromNewBroadcast) {
        if (pmn->IsUpdateRequired()) {
            LogPrint("apollonnode", "CApollonnodePing::CheckAndUpdate -- apollonnode protocol is outdated, apollonnode=%s\n", vin.prevout.ToStringShort());
            return false;
        }

        if (pmn->IsNewStartRequired()) {
            LogPrint("apollonnode", "CApollonnodePing::CheckAndUpdate -- apollonnode is completely expired, new start is required, apollonnode=%s\n", vin.prevout.ToStringShort());
            return false;
        }
    }

    {
        LOCK(cs_main);
        BlockMap::iterator mi = mapBlockApollon.find(blockHash);
        if ((*mi).second && (*mi).second->nHeight < chainActive.Height() - 24) {
            // LogPrintf("CApollonnodePing::CheckAndUpdate -- Apollonnode ping is invalid, block hash is too old: apollonnode=%s  blockHash=%s\n", vin.prevout.ToStringShort(), blockHash.ToString());
            // nDos = 1;
            return false;
        }
    }

    LogPrint("apollonnode", "CApollonnodePing::CheckAndUpdate -- New ping: apollonnode=%s  blockHash=%s  sigTime=%d\n", vin.prevout.ToStringShort(), blockHash.ToString(), sigTime);

    // LogPrintf("mnping - Found corresponding mn for vin: %s\n", vin.prevout.ToStringShort());
    // update only if there is no known ping for this apollonnode or
    // last ping was more then APOLLONNODE_MIN_MNP_SECONDS-60 ago comparing to this one
    if (pmn->IsPingedWithin(APOLLONNODE_MIN_MNP_SECONDS - 60, sigTime)) {
        LogPrint("apollonnode", "CApollonnodePing::CheckAndUpdate -- Apollonnode ping arrived too early, apollonnode=%s\n", vin.prevout.ToStringShort());
        //nDos = 1; //disable, this is happening frequently and causing banned peers
        return false;
    }

    if (!CheckSignature(pmn->pubKeyApollonnode, nDos)) return false;

    // so, ping seems to be ok

    // if we are still syncing and there was no known ping for this mn for quite a while
    // (NOTE: assuming that APOLLONNODE_EXPIRATION_SECONDS/2 should be enough to finish mn list sync)
    if (!apollonnodeSync.IsApollonnodeListSynced() && !pmn->IsPingedWithin(APOLLONNODE_EXPIRATION_SECONDS / 2)) {
        // let's bump sync timeout
        LogPrint("apollonnode", "CApollonnodePing::CheckAndUpdate -- bumping sync timeout, apollonnode=%s\n", vin.prevout.ToStringShort());
        apollonnodeSync.AddedApollonnodeList();
        GetMainSignals().UpdatedApollonnode(*pmn);
    }

    // let's store this ping as the last one
    LogPrint("apollonnode", "CApollonnodePing::CheckAndUpdate -- Apollonnode ping accepted, apollonnode=%s\n", vin.prevout.ToStringShort());
    pmn->SetLastPing(*this);

    // and update mnodeman.mapSeenApollonnodeBroadcast.lastPing which is probably outdated
    CApollonnodeBroadcast mnb(*pmn);
    uint256 hash = mnb.GetHash();
    if (mnodeman.mapSeenApollonnodeBroadcast.count(hash)) {
        mnodeman.mapSeenApollonnodeBroadcast[hash].second.SetLastPing(*this);
    }

    pmn->Check(true); // force update, ignoring cache
    if (!pmn->IsEnabled()) return false;

    LogPrint("apollonnode", "CApollonnodePing::CheckAndUpdate -- Apollonnode ping acceepted and relayed, apollonnode=%s\n", vin.prevout.ToStringShort());
    Relay();

    return true;
}

void CApollonnodePing::Relay() {
    CInv inv(MSG_APOLLONNODE_PING, GetHash());
    RelayInv(inv);
}

//void CApollonnode::AddGovernanceVote(uint256 nGovernanceObjectHash)
//{
//    if(mapGovernanceObjectsVotedOn.count(nGovernanceObjectHash)) {
//        mapGovernanceObjectsVotedOn[nGovernanceObjectHash]++;
//    } else {
//        mapGovernanceObjectsVotedOn.insert(std::make_pair(nGovernanceObjectHash, 1));
//    }
//}

//void CApollonnode::RemoveGovernanceObject(uint256 nGovernanceObjectHash)
//{
//    std::map<uint256, int>::iterator it = mapGovernanceObjectsVotedOn.find(nGovernanceObjectHash);
//    if(it == mapGovernanceObjectsVotedOn.end()) {
//        return;
//    }
//    mapGovernanceObjectsVotedOn.erase(it);
//}

void CApollonnode::UpdateWatchdogVoteTime() {
    LOCK(cs);
    nTimeLastWatchdogVote = GetTime();
}

/**
*   FLAG GOVERNANCE ITEMS AS DIRTY
*
*   - When apollonnode come and go on the network, we must flag the items they voted on to recalc it's cached flags
*
*/
//void CApollonnode::FlagGovernanceItemsAsDirty()
//{
//    std::vector<uint256> vecDirty;
//    {
//        std::map<uint256, int>::iterator it = mapGovernanceObjectsVotedOn.begin();
//        while(it != mapGovernanceObjectsVotedOn.end()) {
//            vecDirty.push_back(it->first);
//            ++it;
//        }
//    }
//    for(size_t i = 0; i < vecDirty.size(); ++i) {
//        mnodeman.AddDirtyGovernanceObjectHash(vecDirty[i]);
//    }
//}
