// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef APOLLONNODEMAN_H
#define APOLLONNODEMAN_H

#include "apollonnode.h"
#include "sync.h"

using namespace std;

class CApollonnodeMan;

extern CApollonnodeMan mnodeman;

/**
 * Provides a forward and reverse apollon between MN vin's and integers.
 *
 * This mapping is normally add-only and is expected to be permanent
 * It is only rebuilt if the size of the apollon exceeds the expected maximum number
 * of MN's and the current number of known MN's.
 *
 * The external interface to this apollon is provided via delegation by CApollonnodeMan
 */
class CApollonnodeApollon
{
public: // Types
    typedef std::map<CTxIn,int> apollon_m_t;

    typedef apollon_m_t::iterator apollon_m_it;

    typedef apollon_m_t::const_iterator apollon_m_cit;

    typedef std::map<int,CTxIn> rapollon_m_t;

    typedef rapollon_m_t::iterator rapollon_m_it;

    typedef rapollon_m_t::const_iterator rapollon_m_cit;

private:
    int                  nSize;

    apollon_m_t            mapApollon;

    rapollon_m_t           mapReverseApollon;

public:
    CApollonnodeApollon();

    int GetSize() const {
        return nSize;
    }

    /// Retrieve apollonnode vin by apollon
    bool Get(int nApollon, CTxIn& vinApollonnode) const;

    /// Get apollon of a apollonnode vin
    int GetApollonnodeApollon(const CTxIn& vinApollonnode) const;

    void AddApollonnodeVIN(const CTxIn& vinApollonnode);

    void Clear();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(mapApollon);
        if(ser_action.ForRead()) {
            RebuildApollon();
        }
    }

private:
    void RebuildApollon();

};

class CApollonnodeMan
{
public:
    typedef std::map<CTxIn,int> apollon_m_t;

    typedef apollon_m_t::iterator apollon_m_it;

    typedef apollon_m_t::const_iterator apollon_m_cit;

private:
    static const int MAX_EXPECTED_APOLLON_SIZE = 30000;

    /// Only allow 1 apollon rebuild per hour
    static const int64_t MIN_APOLLON_REBUILD_TIME = 3600;

    static const std::string SERIALIZATION_VERSION_STRING;

    static const int DSEG_UPDATE_SECONDS        = 3 * 60 * 60;

    static const int LAST_PAID_SCAN_BLOCKS      = 100;

    static const int MIN_POSE_PROTO_VERSION     = 70203;
    static const int MAX_POSE_CONNECTIONS       = 10;
    static const int MAX_POSE_RANK              = 10;
    static const int MAX_POSE_BLOCKS            = 10;

    static const int MNB_RECOVERY_QUORUM_TOTAL      = 10;
    static const int MNB_RECOVERY_QUORUM_REQUIRED   = 6;
    static const int MNB_RECOVERY_MAX_ASK_ENTRIES   = 10;
    static const int MNB_RECOVERY_WAIT_SECONDS      = 60;
    static const int MNB_RECOVERY_RETRY_SECONDS     = 3 * 60 * 60;


    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

    // Keep track of current block apollon
    const CBlockApollon *pCurrentBlockApollon;

    // map to hold all MNs
    std::vector<CApollonnode> vApollonnodes;
    // who's asked for the Apollonnode list and the last time
    std::map<CNetAddr, int64_t> mAskedUsForApollonnodeList;
    // who we asked for the Apollonnode list and the last time
    std::map<CNetAddr, int64_t> mWeAskedForApollonnodeList;
    // which Apollonnodes we've asked for
    std::map<COutPoint, std::map<CNetAddr, int64_t> > mWeAskedForApollonnodeListEntry;
    // who we asked for the apollonnode verification
    std::map<CNetAddr, CApollonnodeVerification> mWeAskedForVerification;

    // these maps are used for apollonnode recovery from APOLLONNODE_NEW_START_REQUIRED state
    std::map<uint256, std::pair< int64_t, std::set<CNetAddr> > > mMnbRecoveryRequests;
    std::map<uint256, std::vector<CApollonnodeBroadcast> > mMnbRecoveryGoodReplies;
    std::list< std::pair<CService, uint256> > listScheduledMnbRequestConnections;

    int64_t nLastApollonRebuildTime;

    CApollonnodeApollon apollonApollonnodes;

    CApollonnodeApollon apollonApollonnodesOld;

    /// Set when apollon has been rebuilt, clear when read
    bool fApollonRebuilt;

    /// Set when apollonnodes are added, cleared when CGovernanceManager is notified
    bool fApollonnodesAdded;

    /// Set when apollonnodes are removed, cleared when CGovernanceManager is notified
    bool fApollonnodesRemoved;

    std::vector<uint256> vecDirtyGovernanceObjectHashes;

    int64_t nLastWatchdogVoteTime;

    friend class CApollonnodeSync;

public:
    // Keep track of all broadcasts I've seen
    std::map<uint256, std::pair<int64_t, CApollonnodeBroadcast> > mapSeenApollonnodeBroadcast;
    // Keep track of all pings I've seen
    std::map<uint256, CApollonnodePing> mapSeenApollonnodePing;
    // Keep track of all verifications I've seen
    std::map<uint256, CApollonnodeVerification> mapSeenApollonnodeVerification;
    // keep track of dsq count to prevent apollonnodes from gaming darksend queue
    int64_t nDsqCount;


    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        LOCK(cs);
        std::string strVersion;
        if(ser_action.ForRead()) {
            READWRITE(strVersion);
        }
        else {
            strVersion = SERIALIZATION_VERSION_STRING; 
            READWRITE(strVersion);
        }

        READWRITE(vApollonnodes);
        READWRITE(mAskedUsForApollonnodeList);
        READWRITE(mWeAskedForApollonnodeList);
        READWRITE(mWeAskedForApollonnodeListEntry);
        READWRITE(mMnbRecoveryRequests);
        READWRITE(mMnbRecoveryGoodReplies);
        READWRITE(nLastWatchdogVoteTime);
        READWRITE(nDsqCount);

        READWRITE(mapSeenApollonnodeBroadcast);
        READWRITE(mapSeenApollonnodePing);
        READWRITE(apollonApollonnodes);
        if(ser_action.ForRead() && (strVersion != SERIALIZATION_VERSION_STRING)) {
            Clear();
        }
    }

    CApollonnodeMan();

    /// Add an entry
    bool Add(CApollonnode &mn);

    /// Ask (source) node for mnb
    void AskForMN(CNode *pnode, const CTxIn &vin);
    void AskForMnb(CNode *pnode, const uint256 &hash);

    /// Check all Apollonnodes
    void Check();

    /// Check all Apollonnodes and remove inactive
    void CheckAndRemove();

    /// Clear Apollonnode vector
    void Clear();

    /// Count Apollonnodes filtered by nProtocolVersion.
    /// Apollonnode nProtocolVersion should match or be above the one specified in param here.
    int CountApollonnodes(int nProtocolVersion = -1);
    /// Count enabled Apollonnodes filtered by nProtocolVersion.
    /// Apollonnode nProtocolVersion should match or be above the one specified in param here.
    int CountEnabled(int nProtocolVersion = -1);

    /// Count Apollonnodes by network type - NET_IPV4, NET_IPV6, NET_TOR
    // int CountByIP(int nNetworkType);

    void DsegUpdate(CNode* pnode);

    /// Find an entry
    CApollonnode* Find(const std::string &txHash, const std::string outputApollon);
    CApollonnode* Find(const CScript &payee);
    CApollonnode* Find(const CTxIn& vin);
    CApollonnode* Find(const CPubKey& pubKeyApollonnode);

    /// Versions of Find that are safe to use from outside the class
    bool Get(const CPubKey& pubKeyApollonnode, CApollonnode& apollonnode);
    bool Get(const CTxIn& vin, CApollonnode& apollonnode);

    /// Retrieve apollonnode vin by apollon
    bool Get(int nApollon, CTxIn& vinApollonnode, bool& fApollonRebuiltOut) {
        LOCK(cs);
        fApollonRebuiltOut = fApollonRebuilt;
        return apollonApollonnodes.Get(nApollon, vinApollonnode);
    }

    bool GetApollonRebuiltFlag() {
        LOCK(cs);
        return fApollonRebuilt;
    }

    /// Get apollon of a apollonnode vin
    int GetApollonnodeApollon(const CTxIn& vinApollonnode) {
        LOCK(cs);
        return apollonApollonnodes.GetApollonnodeApollon(vinApollonnode);
    }

    /// Get old apollon of a apollonnode vin
    int GetApollonnodeApollonOld(const CTxIn& vinApollonnode) {
        LOCK(cs);
        return apollonApollonnodesOld.GetApollonnodeApollon(vinApollonnode);
    }

    /// Get apollonnode VIN for an old apollon value
    bool GetApollonnodeVinForApollonOld(int nApollonnodeApollon, CTxIn& vinApollonnodeOut) {
        LOCK(cs);
        return apollonApollonnodesOld.Get(nApollonnodeApollon, vinApollonnodeOut);
    }

    /// Get apollon of a apollonnode vin, returning rebuild flag
    int GetApollonnodeApollon(const CTxIn& vinApollonnode, bool& fApollonRebuiltOut) {
        LOCK(cs);
        fApollonRebuiltOut = fApollonRebuilt;
        return apollonApollonnodes.GetApollonnodeApollon(vinApollonnode);
    }

    void ClearOldApollonnodeApollon() {
        LOCK(cs);
        apollonApollonnodesOld.Clear();
        fApollonRebuilt = false;
    }

    bool Has(const CTxIn& vin);

    apollonnode_info_t GetApollonnodeInfo(const CTxIn& vin);

    apollonnode_info_t GetApollonnodeInfo(const CPubKey& pubKeyApollonnode);

    char* GetNotQualifyReason(CApollonnode& mn, int nBlockHeight, bool fFilterSigTime, int nMnCount);

    UniValue GetNotQualifyReasonToUniValue(CApollonnode& mn, int nBlockHeight, bool fFilterSigTime, int nMnCount);

    /// Find an entry in the apollonnode list that is next to be paid
    CApollonnode* GetNextApollonnodeInQueueForPayment(int nBlockHeight, bool fFilterSigTime, int& nCount);
    /// Same as above but use current block height
    CApollonnode* GetNextApollonnodeInQueueForPayment(bool fFilterSigTime, int& nCount);

    /// Find a random entry
    CApollonnode* FindRandomNotInVec(const std::vector<CTxIn> &vecToExclude, int nProtocolVersion = -1);

    std::vector<CApollonnode> GetFullApollonnodeVector() { LOCK(cs); return vApollonnodes; }

    std::vector<std::pair<int, CApollonnode> > GetApollonnodeRanks(int nBlockHeight = -1, int nMinProtocol=0);
    int GetApollonnodeRank(const CTxIn &vin, int nBlockHeight, int nMinProtocol=0, bool fOnlyActive=true);
    CApollonnode* GetApollonnodeByRank(int nRank, int nBlockHeight, int nMinProtocol=0, bool fOnlyActive=true);

    void ProcessApollonnodeConnections();
    std::pair<CService, std::set<uint256> > PopScheduledMnbRequestConnection();

    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

    void DoFullVerificationStep();
    void CheckSameAddr();
    bool SendVerifyRequest(const CAddress& addr, const std::vector<CApollonnode*>& vSortedByAddr);
    void SendVerifyReply(CNode* pnode, CApollonnodeVerification& mnv);
    void ProcessVerifyReply(CNode* pnode, CApollonnodeVerification& mnv);
    void ProcessVerifyBroadcast(CNode* pnode, const CApollonnodeVerification& mnv);

    /// Return the number of (unique) Apollonnodes
    int size() { return vApollonnodes.size(); }

    std::string ToString() const;

    /// Update apollonnode list and maps using provided CApollonnodeBroadcast
    void UpdateApollonnodeList(CApollonnodeBroadcast mnb);
    /// Perform complete check and only then update list and maps
    bool CheckMnbAndUpdateApollonnodeList(CNode* pfrom, CApollonnodeBroadcast mnb, int& nDos);
    bool IsMnbRecoveryRequested(const uint256& hash) { return mMnbRecoveryRequests.count(hash); }

    void UpdateLastPaid();

    void CheckAndRebuildApollonnodeApollon();

    void AddDirtyGovernanceObjectHash(const uint256& nHash)
    {
        LOCK(cs);
        vecDirtyGovernanceObjectHashes.push_back(nHash);
    }

    std::vector<uint256> GetAndClearDirtyGovernanceObjectHashes()
    {
        LOCK(cs);
        std::vector<uint256> vecTmp = vecDirtyGovernanceObjectHashes;
        vecDirtyGovernanceObjectHashes.clear();
        return vecTmp;;
    }

    bool IsWatchdogActive();
    void UpdateWatchdogVoteTime(const CTxIn& vin);
    bool AddGovernanceVote(const CTxIn& vin, uint256 nGovernanceObjectHash);
    void RemoveGovernanceObject(uint256 nGovernanceObjectHash);

    void CheckApollonnode(const CTxIn& vin, bool fForce = false);
    void CheckApollonnode(const CPubKey& pubKeyApollonnode, bool fForce = false);

    int GetApollonnodeState(const CTxIn& vin);
    int GetApollonnodeState(const CPubKey& pubKeyApollonnode);

    bool IsApollonnodePingedWithin(const CTxIn& vin, int nSeconds, int64_t nTimeToCheckAt = -1);
    void SetApollonnodeLastPing(const CTxIn& vin, const CApollonnodePing& mnp);

    void UpdatedBlockTip(const CBlockApollon *papollon);

    /**
     * Called to notify CGovernanceManager that the apollonnode apollon has been updated.
     * Must be called while not holding the CApollonnodeMan::cs mutex
     */
    void NotifyApollonnodeUpdates();

};

#endif
