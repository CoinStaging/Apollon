// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef APOLLONNODE_H
#define APOLLONNODE_H

#include "key.h"
#include "main.h"
#include "net.h"
#include "spork.h"
#include "timedata.h"
#include "utiltime.h"

class CApollonnode;
class CApollonnodeBroadcast;
class CApollonnodePing;

static const int APOLLONNODE_CHECK_SECONDS               =   5;
static const int APOLLONNODE_MIN_MNB_SECONDS             =   5 * 60; //BROADCAST_TIME
static const int APOLLONNODE_MIN_MNP_SECONDS             =  10 * 60; //PRE_ENABLE_TIME
static const int APOLLONNODE_EXPIRATION_SECONDS          =  65 * 60;
static const int APOLLONNODE_WATCHDOG_MAX_SECONDS        = 120 * 60;
static const int APOLLONNODE_NEW_START_REQUIRED_SECONDS  = 180 * 60;
static const int APOLLONNODE_COIN_REQUIRED  = 5000;

static const int APOLLONNODE_POSE_BAN_MAX_SCORE          = 5;
//
// The Apollonnode Ping Class : Contains a different serialize method for sending pings from apollonnodes throughout the network
//

class CApollonnodePing
{
public:
    CTxIn vin;
    uint256 blockHash;
    int64_t sigTime; //mnb message times
    std::vector<unsigned char> vchSig;
    //removed stop

    CApollonnodePing() :
        vin(),
        blockHash(),
        sigTime(0),
        vchSig()
        {}

    CApollonnodePing(CTxIn& vinNew);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(vin);
        READWRITE(blockHash);
        READWRITE(sigTime);
        READWRITE(vchSig);
    }

    void swap(CApollonnodePing& first, CApollonnodePing& second) // nothrow
    {
        // enable ADL (not necessary in our case, but good practice)
        using std::swap;

        // by swapping the members of two classes,
        // the two classes are effectively swapped
        swap(first.vin, second.vin);
        swap(first.blockHash, second.blockHash);
        swap(first.sigTime, second.sigTime);
        swap(first.vchSig, second.vchSig);
    }

    uint256 GetHash() const
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << vin;
        ss << sigTime;
        return ss.GetHash();
    }

    bool IsExpired() { return GetTime() - sigTime > APOLLONNODE_NEW_START_REQUIRED_SECONDS; }

    bool Sign(CKey& keyApollonnode, CPubKey& pubKeyApollonnode);
    bool CheckSignature(CPubKey& pubKeyApollonnode, int &nDos);
    bool SimpleCheck(int& nDos);
    bool CheckAndUpdate(CApollonnode* pmn, bool fFromNewBroadcast, int& nDos);
    void Relay();

    CApollonnodePing& operator=(CApollonnodePing from)
    {
        swap(*this, from);
        return *this;
    }
    friend bool operator==(const CApollonnodePing& a, const CApollonnodePing& b)
    {
        return a.vin == b.vin && a.blockHash == b.blockHash;
    }
    friend bool operator!=(const CApollonnodePing& a, const CApollonnodePing& b)
    {
        return !(a == b);
    }

};

struct apollonnode_info_t
{
    apollonnode_info_t()
        : vin(),
          addr(),
          pubKeyCollateralAddress(),
          pubKeyApollonnode(),
          sigTime(0),
          nLastDsq(0),
          nTimeLastChecked(0),
          nTimeLastPaid(0),
          nTimeLastWatchdogVote(0),
          nTimeLastPing(0),
          nActiveState(0),
          nProtocolVersion(0),
          fInfoValid(false),
          nRank(0)
        {}

    CTxIn vin;
    CService addr;
    CPubKey pubKeyCollateralAddress;
    CPubKey pubKeyApollonnode;
    int64_t sigTime; //mnb message time
    int64_t nLastDsq; //the dsq count from the last dsq broadcast of this node
    int64_t nTimeLastChecked;
    int64_t nTimeLastPaid;
    int64_t nTimeLastWatchdogVote;
    int64_t nTimeLastPing;
    int nActiveState;
    int nProtocolVersion;
    bool fInfoValid;
    int nRank;
};

//
// The Apollonnode Class. For managing the Darksend process. It contains the input of the 1000DRK, signature to prove
// it's the one who own that ip address and code for calculating the payment election.
//
class CApollonnode
{
private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

public:
    enum state {
        APOLLONNODE_PRE_ENABLED,
        APOLLONNODE_ENABLED,
        APOLLONNODE_EXPIRED,
        APOLLONNODE_OUTPOINT_SPENT,
        APOLLONNODE_UPDATE_REQUIRED,
        APOLLONNODE_WATCHDOG_EXPIRED,
        APOLLONNODE_NEW_START_REQUIRED,
        APOLLONNODE_POSE_BAN
    };

    CTxIn vin;
    CService addr;
    CPubKey pubKeyCollateralAddress;
    CPubKey pubKeyApollonnode;
    CApollonnodePing lastPing;
    std::vector<unsigned char> vchSig;
    int64_t sigTime; //mnb message time
    int64_t nLastDsq; //the dsq count from the last dsq broadcast of this node
    int64_t nTimeLastChecked;
    int64_t nTimeLastPaid;
    int64_t nTimeLastWatchdogVote;
    int nActiveState;
    int nCacheCollateralBlock;
    int nBlockLastPaid;
    int nProtocolVersion;
    int nPoSeBanScore;
    int nPoSeBanHeight;
    int nRank;
    bool fAllowMixingTx;
    bool fUnitTest;

    // KEEP TRACK OF GOVERNANCE ITEMS EACH APOLLONNODE HAS VOTE UPON FOR RECALCULATION
    std::map<uint256, int> mapGovernanceObjectsVotedOn;

    CApollonnode();
    CApollonnode(const CApollonnode& other);
    CApollonnode(const CApollonnodeBroadcast& mnb);
    CApollonnode(CService addrNew, CTxIn vinNew, CPubKey pubKeyCollateralAddressNew, CPubKey pubKeyApollonnodeNew, int nProtocolVersionIn);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        LOCK(cs);
        READWRITE(vin);
        READWRITE(addr);
        READWRITE(pubKeyCollateralAddress);
        READWRITE(pubKeyApollonnode);
        READWRITE(lastPing);
        READWRITE(vchSig);
        READWRITE(sigTime);
        READWRITE(nLastDsq);
        READWRITE(nTimeLastChecked);
        READWRITE(nTimeLastPaid);
        READWRITE(nTimeLastWatchdogVote);
        READWRITE(nActiveState);
        READWRITE(nCacheCollateralBlock);
        READWRITE(nBlockLastPaid);
        READWRITE(nProtocolVersion);
        READWRITE(nPoSeBanScore);
        READWRITE(nPoSeBanHeight);
        READWRITE(fAllowMixingTx);
        READWRITE(fUnitTest);
        READWRITE(mapGovernanceObjectsVotedOn);
    }

    void swap(CApollonnode& first, CApollonnode& second) // nothrow
    {
        // enable ADL (not necessary in our case, but good practice)
        using std::swap;

        // by swapping the members of two classes,
        // the two classes are effectively swapped
        swap(first.vin, second.vin);
        swap(first.addr, second.addr);
        swap(first.pubKeyCollateralAddress, second.pubKeyCollateralAddress);
        swap(first.pubKeyApollonnode, second.pubKeyApollonnode);
        swap(first.lastPing, second.lastPing);
        swap(first.vchSig, second.vchSig);
        swap(first.sigTime, second.sigTime);
        swap(first.nLastDsq, second.nLastDsq);
        swap(first.nTimeLastChecked, second.nTimeLastChecked);
        swap(first.nTimeLastPaid, second.nTimeLastPaid);
        swap(first.nTimeLastWatchdogVote, second.nTimeLastWatchdogVote);
        swap(first.nActiveState, second.nActiveState);
        swap(first.nRank, second.nRank);
        swap(first.nCacheCollateralBlock, second.nCacheCollateralBlock);
        swap(first.nBlockLastPaid, second.nBlockLastPaid);
        swap(first.nProtocolVersion, second.nProtocolVersion);
        swap(first.nPoSeBanScore, second.nPoSeBanScore);
        swap(first.nPoSeBanHeight, second.nPoSeBanHeight);
        swap(first.fAllowMixingTx, second.fAllowMixingTx);
        swap(first.fUnitTest, second.fUnitTest);
        swap(first.mapGovernanceObjectsVotedOn, second.mapGovernanceObjectsVotedOn);
    }

    // CALCULATE A RANK AGAINST OF GIVEN BLOCK
    arith_uint256 CalculateScore(const uint256& blockHash);

    bool UpdateFromNewBroadcast(CApollonnodeBroadcast& mnb);

    void Check(bool fForce = false);

    bool IsBroadcastedWithin(int nSeconds) { return GetAdjustedTime() - sigTime < nSeconds; }

    bool IsPingedWithin(int nSeconds, int64_t nTimeToCheckAt = -1)
    {
        if(lastPing == CApollonnodePing()) return false;

        if(nTimeToCheckAt == -1) {
            nTimeToCheckAt = GetAdjustedTime();
        }
        return nTimeToCheckAt - lastPing.sigTime < nSeconds;
    }

    bool IsEnabled() { return nActiveState == APOLLONNODE_ENABLED; }
    bool IsPreEnabled() { return nActiveState == APOLLONNODE_PRE_ENABLED; }
    bool IsPoSeBanned() { return nActiveState == APOLLONNODE_POSE_BAN; }
    // NOTE: this one relies on nPoSeBanScore, not on nActiveState as everything else here
    bool IsPoSeVerified() { return nPoSeBanScore <= -APOLLONNODE_POSE_BAN_MAX_SCORE; }
    bool IsExpired() { return nActiveState == APOLLONNODE_EXPIRED; }
    bool IsOutpointSpent() { return nActiveState == APOLLONNODE_OUTPOINT_SPENT; }
    bool IsUpdateRequired() { return nActiveState == APOLLONNODE_UPDATE_REQUIRED; }
    bool IsWatchdogExpired() { return nActiveState == APOLLONNODE_WATCHDOG_EXPIRED; }
    bool IsNewStartRequired() { return nActiveState == APOLLONNODE_NEW_START_REQUIRED; }

    static bool IsValidStateForAutoStart(int nActiveStateIn)
    {
        return  nActiveStateIn == APOLLONNODE_ENABLED ||
                nActiveStateIn == APOLLONNODE_PRE_ENABLED ||
                nActiveStateIn == APOLLONNODE_EXPIRED ||
                nActiveStateIn == APOLLONNODE_WATCHDOG_EXPIRED;
    }

    bool IsValidForPayment();

    bool IsMyApollonnode();

    bool IsValidNetAddr();
    static bool IsValidNetAddr(CService addrIn);

    void IncreasePoSeBanScore() { if(nPoSeBanScore < APOLLONNODE_POSE_BAN_MAX_SCORE) nPoSeBanScore++; }
    void DecreasePoSeBanScore() { if(nPoSeBanScore > -APOLLONNODE_POSE_BAN_MAX_SCORE) nPoSeBanScore--; }

    apollonnode_info_t GetInfo();

    static std::string StateToString(int nStateIn);
    std::string GetStateString() const;
    std::string GetStatus() const;
    std::string ToString() const;
    UniValue ToJSON() const;

    void SetStatus(int newState);
    void SetLastPing(CApollonnodePing newApollonnodePing);
    void SetTimeLastPaid(int64_t newTimeLastPaid);
    void SetBlockLastPaid(int newBlockLastPaid);
    void SetRank(int newRank);

    int GetCollateralAge();

    int GetLastPaidTime() const { return nTimeLastPaid; }
    int GetLastPaidBlock() const { return nBlockLastPaid; }
    void UpdateLastPaid(const CBlockApollon *papollon, int nMaxBlocksToScanBack);

    // KEEP TRACK OF EACH GOVERNANCE ITEM INCASE THIS NODE GOES OFFLINE, SO WE CAN RECALC THEIR STATUS
    void AddGovernanceVote(uint256 nGovernanceObjectHash);
    // RECALCULATE CACHED STATUS FLAGS FOR ALL AFFECTED OBJECTS
    void FlagGovernanceItemsAsDirty();

    void RemoveGovernanceObject(uint256 nGovernanceObjectHash);

    void UpdateWatchdogVoteTime();

    CApollonnode& operator=(CApollonnode from)
    {
        swap(*this, from);
        return *this;
    }
    friend bool operator==(const CApollonnode& a, const CApollonnode& b)
    {
        return a.vin == b.vin;
    }
    friend bool operator!=(const CApollonnode& a, const CApollonnode& b)
    {
        return !(a.vin == b.vin);
    }

};


//
// The Apollonnode Broadcast Class : Contains a different serialize method for sending apollonnodes through the network
//

class CApollonnodeBroadcast : public CApollonnode
{
public:

    bool fRecovery;

    CApollonnodeBroadcast() : CApollonnode(), fRecovery(false) {}
    CApollonnodeBroadcast(const CApollonnode& mn) : CApollonnode(mn), fRecovery(false) {}
    CApollonnodeBroadcast(CService addrNew, CTxIn vinNew, CPubKey pubKeyCollateralAddressNew, CPubKey pubKeyApollonnodeNew, int nProtocolVersionIn) :
        CApollonnode(addrNew, vinNew, pubKeyCollateralAddressNew, pubKeyApollonnodeNew, nProtocolVersionIn), fRecovery(false) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(vin);
        READWRITE(addr);
        READWRITE(pubKeyCollateralAddress);
        READWRITE(pubKeyApollonnode);
        READWRITE(vchSig);
        READWRITE(sigTime);
        READWRITE(nProtocolVersion);
        READWRITE(lastPing);
    }

    uint256 GetHash() const
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << vin;
        ss << pubKeyCollateralAddress;
        ss << sigTime;
        return ss.GetHash();
    }

    /// Create Apollonnode broadcast, needs to be relayed manually after that
    static bool Create(CTxIn vin, CService service, CKey keyCollateralAddressNew, CPubKey pubKeyCollateralAddressNew, CKey keyApollonnodeNew, CPubKey pubKeyApollonnodeNew, std::string &strErrorRet, CApollonnodeBroadcast &mnbRet);
    static bool Create(std::string strService, std::string strKey, std::string strTxHash, std::string strOutputApollon, std::string& strErrorRet, CApollonnodeBroadcast &mnbRet, bool fOffline = false);

    bool SimpleCheck(int& nDos);
    bool Update(CApollonnode* pmn, int& nDos);
    bool CheckOutpoint(int& nDos);

    bool Sign(CKey& keyCollateralAddress);
    bool CheckSignature(int& nDos);
    void RelayApollonNode();
};

class CApollonnodeVerification
{
public:
    CTxIn vin1;
    CTxIn vin2;
    CService addr;
    int nonce;
    int nBlockHeight;
    std::vector<unsigned char> vchSig1;
    std::vector<unsigned char> vchSig2;

    CApollonnodeVerification() :
        vin1(),
        vin2(),
        addr(),
        nonce(0),
        nBlockHeight(0),
        vchSig1(),
        vchSig2()
        {}

    CApollonnodeVerification(CService addr, int nonce, int nBlockHeight) :
        vin1(),
        vin2(),
        addr(addr),
        nonce(nonce),
        nBlockHeight(nBlockHeight),
        vchSig1(),
        vchSig2()
        {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(vin1);
        READWRITE(vin2);
        READWRITE(addr);
        READWRITE(nonce);
        READWRITE(nBlockHeight);
        READWRITE(vchSig1);
        READWRITE(vchSig2);
    }

    uint256 GetHash() const
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << vin1;
        ss << vin2;
        ss << addr;
        ss << nonce;
        ss << nBlockHeight;
        return ss.GetHash();
    }

    void Relay() const
    {
        CInv inv(MSG_APOLLONNODE_VERIFY, GetHash());
        RelayInv(inv);
    }
};

#endif
