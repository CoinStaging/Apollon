// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activeapollonnode.h"
#include "addrman.h"
#include "darksend.h"
//#include "governance.h"
#include "apollonnode-payments.h"
#include "apollonnode-sync.h"
#include "apollonnode.h"
#include "apollonnodeconfig.h"
#include "apollonnodeman.h"
#include "netfulfilledman.h"
#include "util.h"
#include "validationinterface.h"

/** Apollonnode manager */
CApollonnodeMan mnodeman;

const std::string CApollonnodeMan::SERIALIZATION_VERSION_STRING = "CApollonnodeMan-Version-4";

struct CompareLastPaidBlock
{
    bool operator()(const std::pair<int, CApollonnode*>& t1,
                    const std::pair<int, CApollonnode*>& t2) const
    {
        return (t1.first != t2.first) ? (t1.first < t2.first) : (t1.second->vin < t2.second->vin);
    }
};

struct CompareScoreMN
{
    bool operator()(const std::pair<int64_t, CApollonnode*>& t1,
                    const std::pair<int64_t, CApollonnode*>& t2) const
    {
        return (t1.first != t2.first) ? (t1.first < t2.first) : (t1.second->vin < t2.second->vin);
    }
};

CApollonnodeApollon::CApollonnodeApollon()
    : nSize(0),
      mapApollon(),
      mapReverseApollon()
{}

bool CApollonnodeApollon::Get(int nApollon, CTxIn& vinApollonnode) const
{
    rapollon_m_cit it = mapReverseApollon.find(nApollon);
    if(it == mapReverseApollon.end()) {
        return false;
    }
    vinApollonnode = it->second;
    return true;
}

int CApollonnodeApollon::GetApollonnodeApollon(const CTxIn& vinApollonnode) const
{
    apollon_m_cit it = mapApollon.find(vinApollonnode);
    if(it == mapApollon.end()) {
        return -1;
    }
    return it->second;
}

void CApollonnodeApollon::AddApollonnodeVIN(const CTxIn& vinApollonnode)
{
    apollon_m_it it = mapApollon.find(vinApollonnode);
    if(it != mapApollon.end()) {
        return;
    }
    int nNextApollon = nSize;
    mapApollon[vinApollonnode] = nNextApollon;
    mapReverseApollon[nNextApollon] = vinApollonnode;
    ++nSize;
}

void CApollonnodeApollon::Clear()
{
    mapApollon.clear();
    mapReverseApollon.clear();
    nSize = 0;
}
struct CompareByAddr

{
    bool operator()(const CApollonnode* t1,
                    const CApollonnode* t2) const
    {
        return t1->addr < t2->addr;
    }
};

void CApollonnodeApollon::RebuildApollon()
{
    nSize = mapApollon.size();
    for(apollon_m_it it = mapApollon.begin(); it != mapApollon.end(); ++it) {
        mapReverseApollon[it->second] = it->first;
    }
}

CApollonnodeMan::CApollonnodeMan() : cs(),
  vApollonnodes(),
  mAskedUsForApollonnodeList(),
  mWeAskedForApollonnodeList(),
  mWeAskedForApollonnodeListEntry(),
  mWeAskedForVerification(),
  mMnbRecoveryRequests(),
  mMnbRecoveryGoodReplies(),
  listScheduledMnbRequestConnections(),
  nLastApollonRebuildTime(0),
  apollonApollonnodes(),
  apollonApollonnodesOld(),
  fApollonRebuilt(false),
  fApollonnodesAdded(false),
  fApollonnodesRemoved(false),
//  vecDirtyGovernanceObjectHashes(),
  nLastWatchdogVoteTime(0),
  mapSeenApollonnodeBroadcast(),
  mapSeenApollonnodePing(),
  nDsqCount(0)
{}

bool CApollonnodeMan::Add(CApollonnode &mn)
{
    LOCK(cs);

    CApollonnode *pmn = Find(mn.vin);
    if (pmn == NULL) {
        LogPrint("apollonnode", "CApollonnodeMan::Add -- Adding new Apollonnode: addr=%s, %i now\n", mn.addr.ToString(), size() + 1);
        vApollonnodes.push_back(mn);
        apollonApollonnodes.AddApollonnodeVIN(mn.vin);
        fApollonnodesAdded = true;
        return true;
    }

    return false;
}

void CApollonnodeMan::AskForMN(CNode* pnode, const CTxIn &vin)
{
    if(!pnode) return;

    LOCK(cs);

    std::map<COutPoint, std::map<CNetAddr, int64_t> >::iterator it1 = mWeAskedForApollonnodeListEntry.find(vin.prevout);
    if (it1 != mWeAskedForApollonnodeListEntry.end()) {
        std::map<CNetAddr, int64_t>::iterator it2 = it1->second.find(pnode->addr);
        if (it2 != it1->second.end()) {
            if (GetTime() < it2->second) {
                // we've asked recently, should not repeat too often or we could get banned
                return;
            }
            // we asked this node for this outpoint but it's ok to ask again already
            LogPrintf("CApollonnodeMan::AskForMN -- Asking same peer %s for missing apollonnode entry again: %s\n", pnode->addr.ToString(), vin.prevout.ToStringShort());
        } else {
            // we already asked for this outpoint but not this node
            LogPrintf("CApollonnodeMan::AskForMN -- Asking new peer %s for missing apollonnode entry: %s\n", pnode->addr.ToString(), vin.prevout.ToStringShort());
        }
    } else {
        // we never asked any node for this outpoint
        LogPrintf("CApollonnodeMan::AskForMN -- Asking peer %s for missing apollonnode entry for the first time: %s\n", pnode->addr.ToString(), vin.prevout.ToStringShort());
    }
    mWeAskedForApollonnodeListEntry[vin.prevout][pnode->addr] = GetTime() + DSEG_UPDATE_SECONDS;

    pnode->PushMessage(NetMsgType::DSEG, vin);
}

void CApollonnodeMan::Check()
{
    LOCK(cs);

//    LogPrint("apollonnode", "CApollonnodeMan::Check -- nLastWatchdogVoteTime=%d, IsWatchdogActive()=%d\n", nLastWatchdogVoteTime, IsWatchdogActive());

    BOOST_FOREACH(CApollonnode& mn, vApollonnodes) {
        mn.Check();
    }
}

void CApollonnodeMan::CheckAndRemove()
{
    if(!apollonnodeSync.IsApollonnodeListSynced()) return;

    LogPrintf("CApollonnodeMan::CheckAndRemove\n");

    {
        // Need LOCK2 here to ensure consistent locking order because code below locks cs_main
        // in CheckMnbAndUpdateApollonnodeList()
        LOCK2(cs_main, cs);

        Check();

        // Remove spent apollonnodes, prepare structures and make requests to reasure the state of inactive ones
        std::vector<CApollonnode>::iterator it = vApollonnodes.begin();
        std::vector<std::pair<int, CApollonnode> > vecApollonnodeRanks;
        // ask for up to MNB_RECOVERY_MAX_ASK_ENTRIES apollonnode entries at a time
        int nAskForMnbRecovery = MNB_RECOVERY_MAX_ASK_ENTRIES;
        while(it != vApollonnodes.end()) {
            CApollonnodeBroadcast mnb = CApollonnodeBroadcast(*it);
            uint256 hash = mnb.GetHash();
            // If collateral was spent ...
            if ((*it).IsOutpointSpent()) {
                LogPrint("apollonnode", "CApollonnodeMan::CheckAndRemove -- Removing Apollonnode: %s  addr=%s  %i now\n", (*it).GetStateString(), (*it).addr.ToString(), size() - 1);

                // erase all of the broadcasts we've seen from this txin, ...
                mapSeenApollonnodeBroadcast.erase(hash);
                mWeAskedForApollonnodeListEntry.erase((*it).vin.prevout);

                // and finally remove it from the list
//                it->FlagGovernanceItemsAsDirty();
                it = vApollonnodes.erase(it);
                fApollonnodesRemoved = true;
            } else {
                bool fAsk = pCurrentBlockApollon &&
                            (nAskForMnbRecovery > 0) &&
                            apollonnodeSync.IsSynced() &&
                            it->IsNewStartRequired() &&
                            !IsMnbRecoveryRequested(hash);
                if(fAsk) {
                    // this mn is in a non-recoverable state and we haven't asked other nodes yet
                    std::set<CNetAddr> setRequested;
                    // calulate only once and only when it's needed
                    if(vecApollonnodeRanks.empty()) {
                        int nRandomBlockHeight = GetRandInt(pCurrentBlockApollon->nHeight);
                        vecApollonnodeRanks = GetApollonnodeRanks(nRandomBlockHeight);
                    }
                    bool fAskedForMnbRecovery = false;
                    // ask first MNB_RECOVERY_QUORUM_TOTAL apollonnodes we can connect to and we haven't asked recently
                    for(int i = 0; setRequested.size() < MNB_RECOVERY_QUORUM_TOTAL && i < (int)vecApollonnodeRanks.size(); i++) {
                        // avoid banning
                        if(mWeAskedForApollonnodeListEntry.count(it->vin.prevout) && mWeAskedForApollonnodeListEntry[it->vin.prevout].count(vecApollonnodeRanks[i].second.addr)) continue;
                        // didn't ask recently, ok to ask now
                        CService addr = vecApollonnodeRanks[i].second.addr;
                        setRequested.insert(addr);
                        listScheduledMnbRequestConnections.push_back(std::make_pair(addr, hash));
                        fAskedForMnbRecovery = true;
                    }
                    if(fAskedForMnbRecovery) {
                        LogPrint("apollonnode", "CApollonnodeMan::CheckAndRemove -- Recovery initiated, apollonnode=%s\n", it->vin.prevout.ToStringShort());
                        nAskForMnbRecovery--;
                    }
                    // wait for mnb recovery replies for MNB_RECOVERY_WAIT_SECONDS seconds
                    mMnbRecoveryRequests[hash] = std::make_pair(GetTime() + MNB_RECOVERY_WAIT_SECONDS, setRequested);
                }
                ++it;
            }
        }

        // proces replies for APOLLONNODE_NEW_START_REQUIRED apollonnodes
        LogPrint("apollonnode", "CApollonnodeMan::CheckAndRemove -- mMnbRecoveryGoodReplies size=%d\n", (int)mMnbRecoveryGoodReplies.size());
        std::map<uint256, std::vector<CApollonnodeBroadcast> >::iterator itMnbReplies = mMnbRecoveryGoodReplies.begin();
        while(itMnbReplies != mMnbRecoveryGoodReplies.end()){
            if(mMnbRecoveryRequests[itMnbReplies->first].first < GetTime()) {
                // all nodes we asked should have replied now
                if(itMnbReplies->second.size() >= MNB_RECOVERY_QUORUM_REQUIRED) {
                    // majority of nodes we asked agrees that this mn doesn't require new mnb, reprocess one of new mnbs
                    LogPrint("apollonnode", "CApollonnodeMan::CheckAndRemove -- reprocessing mnb, apollonnode=%s\n", itMnbReplies->second[0].vin.prevout.ToStringShort());
                    // mapSeenApollonnodeBroadcast.erase(itMnbReplies->first);
                    int nDos;
                    itMnbReplies->second[0].fRecovery = true;
                    CheckMnbAndUpdateApollonnodeList(NULL, itMnbReplies->second[0], nDos);
                }
                LogPrint("apollonnode", "CApollonnodeMan::CheckAndRemove -- removing mnb recovery reply, apollonnode=%s, size=%d\n", itMnbReplies->second[0].vin.prevout.ToStringShort(), (int)itMnbReplies->second.size());
                mMnbRecoveryGoodReplies.erase(itMnbReplies++);
            } else {
                ++itMnbReplies;
            }
        }
    }
    {
        // no need for cm_main below
        LOCK(cs);

        std::map<uint256, std::pair< int64_t, std::set<CNetAddr> > >::iterator itMnbRequest = mMnbRecoveryRequests.begin();
        while(itMnbRequest != mMnbRecoveryRequests.end()){
            // Allow this mnb to be re-verified again after MNB_RECOVERY_RETRY_SECONDS seconds
            // if mn is still in APOLLONNODE_NEW_START_REQUIRED state.
            if(GetTime() - itMnbRequest->second.first > MNB_RECOVERY_RETRY_SECONDS) {
                mMnbRecoveryRequests.erase(itMnbRequest++);
            } else {
                ++itMnbRequest;
            }
        }

        // check who's asked for the Apollonnode list
        std::map<CNetAddr, int64_t>::iterator it1 = mAskedUsForApollonnodeList.begin();
        while(it1 != mAskedUsForApollonnodeList.end()){
            if((*it1).second < GetTime()) {
                mAskedUsForApollonnodeList.erase(it1++);
            } else {
                ++it1;
            }
        }

        // check who we asked for the Apollonnode list
        it1 = mWeAskedForApollonnodeList.begin();
        while(it1 != mWeAskedForApollonnodeList.end()){
            if((*it1).second < GetTime()){
                mWeAskedForApollonnodeList.erase(it1++);
            } else {
                ++it1;
            }
        }

        // check which Apollonnodes we've asked for
        std::map<COutPoint, std::map<CNetAddr, int64_t> >::iterator it2 = mWeAskedForApollonnodeListEntry.begin();
        while(it2 != mWeAskedForApollonnodeListEntry.end()){
            std::map<CNetAddr, int64_t>::iterator it3 = it2->second.begin();
            while(it3 != it2->second.end()){
                if(it3->second < GetTime()){
                    it2->second.erase(it3++);
                } else {
                    ++it3;
                }
            }
            if(it2->second.empty()) {
                mWeAskedForApollonnodeListEntry.erase(it2++);
            } else {
                ++it2;
            }
        }

        std::map<CNetAddr, CApollonnodeVerification>::iterator it3 = mWeAskedForVerification.begin();
        while(it3 != mWeAskedForVerification.end()){
            if(it3->second.nBlockHeight < pCurrentBlockApollon->nHeight - MAX_POSE_BLOCKS) {
                mWeAskedForVerification.erase(it3++);
            } else {
                ++it3;
            }
        }

        // NOTE: do not expire mapSeenApollonnodeBroadcast entries here, clean them on mnb updates!

        // remove expired mapSeenApollonnodePing
        std::map<uint256, CApollonnodePing>::iterator it4 = mapSeenApollonnodePing.begin();
        while(it4 != mapSeenApollonnodePing.end()){
            if((*it4).second.IsExpired()) {
                LogPrint("apollonnode", "CApollonnodeMan::CheckAndRemove -- Removing expired Apollonnode ping: hash=%s\n", (*it4).second.GetHash().ToString());
                mapSeenApollonnodePing.erase(it4++);
            } else {
                ++it4;
            }
        }

        // remove expired mapSeenApollonnodeVerification
        std::map<uint256, CApollonnodeVerification>::iterator itv2 = mapSeenApollonnodeVerification.begin();
        while(itv2 != mapSeenApollonnodeVerification.end()){
            if((*itv2).second.nBlockHeight < pCurrentBlockApollon->nHeight - MAX_POSE_BLOCKS){
                LogPrint("apollonnode", "CApollonnodeMan::CheckAndRemove -- Removing expired Apollonnode verification: hash=%s\n", (*itv2).first.ToString());
                mapSeenApollonnodeVerification.erase(itv2++);
            } else {
                ++itv2;
            }
        }

        LogPrintf("CApollonnodeMan::CheckAndRemove -- %s\n", ToString());

        if(fApollonnodesRemoved) {
            CheckAndRebuildApollonnodeApollon();
        }
    }

    if(fApollonnodesRemoved) {
        NotifyApollonnodeUpdates();
    }
}

void CApollonnodeMan::Clear()
{
    LOCK(cs);
    vApollonnodes.clear();
    mAskedUsForApollonnodeList.clear();
    mWeAskedForApollonnodeList.clear();
    mWeAskedForApollonnodeListEntry.clear();
    mapSeenApollonnodeBroadcast.clear();
    mapSeenApollonnodePing.clear();
    nDsqCount = 0;
    nLastWatchdogVoteTime = 0;
    apollonApollonnodes.Clear();
    apollonApollonnodesOld.Clear();
}

int CApollonnodeMan::CountApollonnodes(int nProtocolVersion)
{
    LOCK(cs);
    int nCount = 0;
    nProtocolVersion = nProtocolVersion == -1 ? mnpayments.GetMinApollonnodePaymentsProto() : nProtocolVersion;

    BOOST_FOREACH(CApollonnode& mn, vApollonnodes) {
        if(mn.nProtocolVersion < nProtocolVersion) continue;
        nCount++;
    }

    return nCount;
}

int CApollonnodeMan::CountEnabled(int nProtocolVersion)
{
    LOCK(cs);
    int nCount = 0;
    nProtocolVersion = nProtocolVersion == -1 ? mnpayments.GetMinApollonnodePaymentsProto() : nProtocolVersion;

    BOOST_FOREACH(CApollonnode& mn, vApollonnodes) {
        if(mn.nProtocolVersion < nProtocolVersion || !mn.IsEnabled()) continue;
        nCount++;
    }

    return nCount;
}

/* Only IPv4 apollonnodes are allowed in 12.1, saving this for later
int CApollonnodeMan::CountByIP(int nNetworkType)
{
    LOCK(cs);
    int nNodeCount = 0;

    BOOST_FOREACH(CApollonnode& mn, vApollonnodes)
        if ((nNetworkType == NET_IPV4 && mn.addr.IsIPv4()) ||
            (nNetworkType == NET_TOR  && mn.addr.IsTor())  ||
            (nNetworkType == NET_IPV6 && mn.addr.IsIPv6())) {
                nNodeCount++;
        }

    return nNodeCount;
}
*/

void CApollonnodeMan::DsegUpdate(CNode* pnode)
{
    LOCK(cs);

    if(Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if(!(pnode->addr.IsRFC1918() || pnode->addr.IsLocal())) {
            std::map<CNetAddr, int64_t>::iterator it = mWeAskedForApollonnodeList.find(pnode->addr);
            if(it != mWeAskedForApollonnodeList.end() && GetTime() < (*it).second) {
                LogPrintf("CApollonnodeMan::DsegUpdate -- we already asked %s for the list; skipping...\n", pnode->addr.ToString());
                return;
            }
        }
    }
    
    pnode->PushMessage(NetMsgType::DSEG, CTxIn());
    int64_t askAgain = GetTime() + DSEG_UPDATE_SECONDS;
    mWeAskedForApollonnodeList[pnode->addr] = askAgain;

    LogPrint("apollonnode", "CApollonnodeMan::DsegUpdate -- asked %s for the list\n", pnode->addr.ToString());
}

CApollonnode* CApollonnodeMan::Find(const std::string &txHash, const std::string outputApollon)
{
    LOCK(cs);

    BOOST_FOREACH(CApollonnode& mn, vApollonnodes)
    {
        COutPoint outpoint = mn.vin.prevout;

        if(txHash==outpoint.hash.ToString().substr(0,64) &&
           outputApollon==to_string(outpoint.n))
            return &mn;
    }
    return NULL;
}

CApollonnode* CApollonnodeMan::Find(const CScript &payee)
{
    LOCK(cs);

    BOOST_FOREACH(CApollonnode& mn, vApollonnodes)
    {
        if(GetScriptForDestination(mn.pubKeyCollateralAddress.GetID()) == payee)
            return &mn;
    }
    return NULL;
}

CApollonnode* CApollonnodeMan::Find(const CTxIn &vin)
{
    LOCK(cs);

    BOOST_FOREACH(CApollonnode& mn, vApollonnodes)
    {
        if(mn.vin.prevout == vin.prevout)
            return &mn;
    }
    return NULL;
}

CApollonnode* CApollonnodeMan::Find(const CPubKey &pubKeyApollonnode)
{
    LOCK(cs);

    BOOST_FOREACH(CApollonnode& mn, vApollonnodes)
    {
        if(mn.pubKeyApollonnode == pubKeyApollonnode)
            return &mn;
    }
    return NULL;
}

bool CApollonnodeMan::Get(const CPubKey& pubKeyApollonnode, CApollonnode& apollonnode)
{
    // Theses mutexes are recursive so double locking by the same thread is safe.
    LOCK(cs);
    CApollonnode* pMN = Find(pubKeyApollonnode);
    if(!pMN)  {
        return false;
    }
    apollonnode = *pMN;
    return true;
}

bool CApollonnodeMan::Get(const CTxIn& vin, CApollonnode& apollonnode)
{
    // Theses mutexes are recursive so double locking by the same thread is safe.
    LOCK(cs);
    CApollonnode* pMN = Find(vin);
    if(!pMN)  {
        return false;
    }
    apollonnode = *pMN;
    return true;
}

apollonnode_info_t CApollonnodeMan::GetApollonnodeInfo(const CTxIn& vin)
{
    apollonnode_info_t info;
    LOCK(cs);
    CApollonnode* pMN = Find(vin);
    if(!pMN)  {
        return info;
    }
    info = pMN->GetInfo();
    return info;
}

apollonnode_info_t CApollonnodeMan::GetApollonnodeInfo(const CPubKey& pubKeyApollonnode)
{
    apollonnode_info_t info;
    LOCK(cs);
    CApollonnode* pMN = Find(pubKeyApollonnode);
    if(!pMN)  {
        return info;
    }
    info = pMN->GetInfo();
    return info;
}

bool CApollonnodeMan::Has(const CTxIn& vin)
{
    LOCK(cs);
    CApollonnode* pMN = Find(vin);
    return (pMN != NULL);
}

char* CApollonnodeMan::GetNotQualifyReason(CApollonnode& mn, int nBlockHeight, bool fFilterSigTime, int nMnCount)
{
    if (!mn.IsValidForPayment()) {
        char* reasonStr = new char[256];
        sprintf(reasonStr, "false: 'not valid for payment'");
        return reasonStr;
    }
    // //check protocol version
    if (mn.nProtocolVersion < mnpayments.GetMinApollonnodePaymentsProto()) {
        // LogPrintf("Invalid nProtocolVersion!\n");
        // LogPrintf("mn.nProtocolVersion=%s!\n", mn.nProtocolVersion);
        // LogPrintf("mnpayments.GetMinApollonnodePaymentsProto=%s!\n", mnpayments.GetMinApollonnodePaymentsProto());
        char* reasonStr = new char[256];
        sprintf(reasonStr, "false: 'Invalid nProtocolVersion', nProtocolVersion=%d", mn.nProtocolVersion);
        return reasonStr;
    }
    //it's in the list (up to 8 entries ahead of current block to allow propagation) -- so let's skip it
    if (mnpayments.IsScheduled(mn, nBlockHeight)) {
        // LogPrintf("mnpayments.IsScheduled!\n");
        char* reasonStr = new char[256];
        sprintf(reasonStr, "false: 'is scheduled'");
        return reasonStr;
    }
    //it's too new, wait for a cycle
    if (fFilterSigTime && mn.sigTime + (nMnCount * 2.6 * 60) > GetAdjustedTime()) {
        // LogPrintf("it's too new, wait for a cycle!\n");
        char* reasonStr = new char[256];
        sprintf(reasonStr, "false: 'too new', sigTime=%s, will be qualifed after=%s",
                DateTimeStrFormat("%Y-%m-%d %H:%M UTC", mn.sigTime).c_str(), DateTimeStrFormat("%Y-%m-%d %H:%M UTC", mn.sigTime + (nMnCount * 2.6 * 60)).c_str());
        return reasonStr;
    }
    //make sure it has at least as many confirmations as there are apollonnodes
    if (mn.GetCollateralAge() < nMnCount) {
        // LogPrintf("mn.GetCollateralAge()=%s!\n", mn.GetCollateralAge());
        // LogPrintf("nMnCount=%s!\n", nMnCount);
        char* reasonStr = new char[256];
        sprintf(reasonStr, "false: 'collateralAge < znCount', collateralAge=%d, znCount=%d", mn.GetCollateralAge(), nMnCount);
        return reasonStr;
    }
    return NULL;
}

// Same method, different return type, to avoid Apollonnode operator issues.
// TODO: discuss standardizing the JSON type here, as it's done everywhere else in the code.
UniValue CApollonnodeMan::GetNotQualifyReasonToUniValue(CApollonnode& mn, int nBlockHeight, bool fFilterSigTime, int nMnCount)
{
    UniValue ret(UniValue::VOBJ);
    UniValue data(UniValue::VOBJ);
    string description;

    if (!mn.IsValidForPayment()) {
        description = "not valid for payment";
    }

    //it's in the list (up to 8 entries ahead of current block to allow propagation) -- so let's skip it
    else if (mnpayments.IsScheduled(mn, nBlockHeight)) {
        description = "Is scheduled";
    }

    // //check protocol version
    else if (mn.nProtocolVersion < mnpayments.GetMinApollonnodePaymentsProto()) {
        description = "Invalid nProtocolVersion";

        data.push_back(Pair("nProtocolVersion", mn.nProtocolVersion));
    }

    //it's too new, wait for a cycle
    else if (fFilterSigTime && mn.sigTime + (nMnCount * 2.6 * 60) > GetAdjustedTime()) {
        // LogPrintf("it's too new, wait for a cycle!\n");
        description = "Too new";

        //TODO unix timestamp
        data.push_back(Pair("sigTime", mn.sigTime));
        data.push_back(Pair("qualifiedAfter", mn.sigTime + (nMnCount * 2.6 * 60)));
    }
    //make sure it has at least as many confirmations as there are apollonnodes
    else if (mn.GetCollateralAge() < nMnCount) {
        description = "collateralAge < znCount";

        data.push_back(Pair("collateralAge", mn.GetCollateralAge()));
        data.push_back(Pair("znCount", nMnCount));
    }

    ret.push_back(Pair("result", description.empty()));
    if(!description.empty()){
        ret.push_back(Pair("description", description));
    }
    if(!data.empty()){
        ret.push_back(Pair("data", data));
    }

    return ret;
}

//
// Deterministically select the oldest/best apollonnode to pay on the network
//
CApollonnode* CApollonnodeMan::GetNextApollonnodeInQueueForPayment(bool fFilterSigTime, int& nCount)
{
    if(!pCurrentBlockApollon) {
        nCount = 0;
        return NULL;
    }
    return GetNextApollonnodeInQueueForPayment(pCurrentBlockApollon->nHeight, fFilterSigTime, nCount);
}

CApollonnode* CApollonnodeMan::GetNextApollonnodeInQueueForPayment(int nBlockHeight, bool fFilterSigTime, int& nCount)
{
    // Need LOCK2 here to ensure consistent locking order because the GetBlockHash call below locks cs_main
    LOCK2(cs_main,cs);

    CApollonnode *pBestApollonnode = NULL;
    std::vector<std::pair<int, CApollonnode*> > vecApollonnodeLastPaid;

    /*
        Make a vector with all of the last paid times
    */
    int nMnCount = CountEnabled();
    int apollon = 0;
    BOOST_FOREACH(CApollonnode &mn, vApollonnodes)
    {
        apollon += 1;
        // LogPrintf("apollon=%s, mn=%s\n", apollon, mn.ToString());
        /*if (!mn.IsValidForPayment()) {
            LogPrint("apollonnodeman", "Apollonnode, %s, addr(%s), not-qualified: 'not valid for payment'\n",
                     mn.vin.prevout.ToStringShort(), CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString());
            continue;
        }
        // //check protocol version
        if (mn.nProtocolVersion < mnpayments.GetMinApollonnodePaymentsProto()) {
            // LogPrintf("Invalid nProtocolVersion!\n");
            // LogPrintf("mn.nProtocolVersion=%s!\n", mn.nProtocolVersion);
            // LogPrintf("mnpayments.GetMinApollonnodePaymentsProto=%s!\n", mnpayments.GetMinApollonnodePaymentsProto());
            LogPrint("apollonnodeman", "Apollonnode, %s, addr(%s), not-qualified: 'invalid nProtocolVersion'\n",
                     mn.vin.prevout.ToStringShort(), CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString());
            continue;
        }
        //it's in the list (up to 8 entries ahead of current block to allow propagation) -- so let's skip it
        if (mnpayments.IsScheduled(mn, nBlockHeight)) {
            // LogPrintf("mnpayments.IsScheduled!\n");
            LogPrint("apollonnodeman", "Apollonnode, %s, addr(%s), not-qualified: 'IsScheduled'\n",
                     mn.vin.prevout.ToStringShort(), CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString());
            continue;
        }
        //it's too new, wait for a cycle
        if (fFilterSigTime && mn.sigTime + (nMnCount * 2.6 * 60) > GetAdjustedTime()) {
            // LogPrintf("it's too new, wait for a cycle!\n");
            LogPrint("apollonnodeman", "Apollonnode, %s, addr(%s), not-qualified: 'it's too new, wait for a cycle!', sigTime=%s, will be qualifed after=%s\n",
                     mn.vin.prevout.ToStringShort(), CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString(), DateTimeStrFormat("%Y-%m-%d %H:%M UTC", mn.sigTime).c_str(), DateTimeStrFormat("%Y-%m-%d %H:%M UTC", mn.sigTime + (nMnCount * 2.6 * 60)).c_str());
            continue;
        }
        //make sure it has at least as many confirmations as there are apollonnodes
        if (mn.GetCollateralAge() < nMnCount) {
            // LogPrintf("mn.GetCollateralAge()=%s!\n", mn.GetCollateralAge());
            // LogPrintf("nMnCount=%s!\n", nMnCount);
            LogPrint("apollonnodeman", "Apollonnode, %s, addr(%s), not-qualified: 'mn.GetCollateralAge() < nMnCount', CollateralAge=%d, nMnCount=%d\n",
                     mn.vin.prevout.ToStringShort(), CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString(), mn.GetCollateralAge(), nMnCount);
            continue;
        }*/
        char* reasonStr = GetNotQualifyReason(mn, nBlockHeight, fFilterSigTime, nMnCount);
        if (reasonStr != NULL) {
            LogPrint("apollonnodeman", "Apollonnode, %s, addr(%s), qualify %s\n",
                     mn.vin.prevout.ToStringShort(), CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString(), reasonStr);
            delete [] reasonStr;
            continue;
        }
        vecApollonnodeLastPaid.push_back(std::make_pair(mn.GetLastPaidBlock(), &mn));
    }
    nCount = (int)vecApollonnodeLastPaid.size();

    //when the network is in the process of upgrading, don't penalize nodes that recently restarted
    if(fFilterSigTime && nCount < nMnCount / 3) {
        // LogPrintf("Need Return, nCount=%s, nMnCount/3=%s\n", nCount, nMnCount/3);
        return GetNextApollonnodeInQueueForPayment(nBlockHeight, false, nCount);
    }

    // Sort them low to high
    sort(vecApollonnodeLastPaid.begin(), vecApollonnodeLastPaid.end(), CompareLastPaidBlock());

    uint256 blockHash;
    if(!GetBlockHash(blockHash, nBlockHeight - 101)) {
        LogPrintf("CApollonnode::GetNextApollonnodeInQueueForPayment -- ERROR: GetBlockHash() failed at nBlockHeight %d\n", nBlockHeight - 101);
        return NULL;
    }
    // Look at 1/10 of the oldest nodes (by last payment), calculate their scores and pay the best one
    //  -- This doesn't look at who is being paid in the +8-10 blocks, allowing for double payments very rarely
    //  -- 1/100 payments should be a double payment on mainnet - (1/(3000/10))*2
    //  -- (chance per block * chances before IsScheduled will fire)
    int nTenthNetwork = nMnCount/10;
    int nCountTenth = 0;
    arith_uint256 nHighest = 0;
    BOOST_FOREACH (PAIRTYPE(int, CApollonnode*)& s, vecApollonnodeLastPaid){
        arith_uint256 nScore = s.second->CalculateScore(blockHash);
        if(nScore > nHighest){
            nHighest = nScore;
            pBestApollonnode = s.second;
        }
        nCountTenth++;
        if(nCountTenth >= nTenthNetwork) break;
    }
    return pBestApollonnode;
}

CApollonnode* CApollonnodeMan::FindRandomNotInVec(const std::vector<CTxIn> &vecToExclude, int nProtocolVersion)
{
    LOCK(cs);

    nProtocolVersion = nProtocolVersion == -1 ? mnpayments.GetMinApollonnodePaymentsProto() : nProtocolVersion;

    int nCountEnabled = CountEnabled(nProtocolVersion);
    int nCountNotExcluded = nCountEnabled - vecToExclude.size();

    LogPrintf("CApollonnodeMan::FindRandomNotInVec -- %d enabled apollonnodes, %d apollonnodes to choose from\n", nCountEnabled, nCountNotExcluded);
    if(nCountNotExcluded < 1) return NULL;

    // fill a vector of pointers
    std::vector<CApollonnode*> vpApollonnodesShuffled;
    BOOST_FOREACH(CApollonnode &mn, vApollonnodes) {
        vpApollonnodesShuffled.push_back(&mn);
    }

    InsecureRand insecureRand;
    // shuffle pointers
    std::random_shuffle(vpApollonnodesShuffled.begin(), vpApollonnodesShuffled.end(), insecureRand);
    bool fExclude;

    // loop through
    BOOST_FOREACH(CApollonnode* pmn, vpApollonnodesShuffled) {
        if(pmn->nProtocolVersion < nProtocolVersion || !pmn->IsEnabled()) continue;
        fExclude = false;
        BOOST_FOREACH(const CTxIn &txinToExclude, vecToExclude) {
            if(pmn->vin.prevout == txinToExclude.prevout) {
                fExclude = true;
                break;
            }
        }
        if(fExclude) continue;
        // found the one not in vecToExclude
        LogPrint("apollonnode", "CApollonnodeMan::FindRandomNotInVec -- found, apollonnode=%s\n", pmn->vin.prevout.ToStringShort());
        return pmn;
    }

    LogPrint("apollonnode", "CApollonnodeMan::FindRandomNotInVec -- failed\n");
    return NULL;
}

int CApollonnodeMan::GetApollonnodeRank(const CTxIn& vin, int nBlockHeight, int nMinProtocol, bool fOnlyActive)
{
    std::vector<std::pair<int64_t, CApollonnode*> > vecApollonnodeScores;

    //make sure we know about this block
    uint256 blockHash = uint256();
    if(!GetBlockHash(blockHash, nBlockHeight)) return -1;

    LOCK(cs);

    // scan for winner
    BOOST_FOREACH(CApollonnode& mn, vApollonnodes) {
        if(mn.nProtocolVersion < nMinProtocol) continue;
        if(fOnlyActive) {
            if(!mn.IsEnabled()) continue;
        }
        else {
            if(!mn.IsValidForPayment()) continue;
        }
        int64_t nScore = mn.CalculateScore(blockHash).GetCompact(false);

        vecApollonnodeScores.push_back(std::make_pair(nScore, &mn));
    }

    sort(vecApollonnodeScores.rbegin(), vecApollonnodeScores.rend(), CompareScoreMN());

    int nRank = 0;
    BOOST_FOREACH (PAIRTYPE(int64_t, CApollonnode*)& scorePair, vecApollonnodeScores) {
        nRank++;
        if(scorePair.second->vin.prevout == vin.prevout) return nRank;
    }

    return -1;
}

std::vector<std::pair<int, CApollonnode> > CApollonnodeMan::GetApollonnodeRanks(int nBlockHeight, int nMinProtocol)
{
    std::vector<std::pair<int64_t, CApollonnode*> > vecApollonnodeScores;
    std::vector<std::pair<int, CApollonnode> > vecApollonnodeRanks;

    //make sure we know about this block
    uint256 blockHash = uint256();
    if(!GetBlockHash(blockHash, nBlockHeight)) return vecApollonnodeRanks;

    LOCK(cs);

    // scan for winner
    BOOST_FOREACH(CApollonnode& mn, vApollonnodes) {

        if(mn.nProtocolVersion < nMinProtocol || !mn.IsEnabled()) continue;

        int64_t nScore = mn.CalculateScore(blockHash).GetCompact(false);

        vecApollonnodeScores.push_back(std::make_pair(nScore, &mn));
    }

    sort(vecApollonnodeScores.rbegin(), vecApollonnodeScores.rend(), CompareScoreMN());

    int nRank = 0;
    BOOST_FOREACH (PAIRTYPE(int64_t, CApollonnode*)& s, vecApollonnodeScores) {
        nRank++;
        s.second->SetRank(nRank);
        vecApollonnodeRanks.push_back(std::make_pair(nRank, *s.second));
    }

    return vecApollonnodeRanks;
}

CApollonnode* CApollonnodeMan::GetApollonnodeByRank(int nRank, int nBlockHeight, int nMinProtocol, bool fOnlyActive)
{
    std::vector<std::pair<int64_t, CApollonnode*> > vecApollonnodeScores;

    LOCK(cs);

    uint256 blockHash;
    if(!GetBlockHash(blockHash, nBlockHeight)) {
        LogPrintf("CApollonnode::GetApollonnodeByRank -- ERROR: GetBlockHash() failed at nBlockHeight %d\n", nBlockHeight);
        return NULL;
    }

    // Fill scores
    BOOST_FOREACH(CApollonnode& mn, vApollonnodes) {

        if(mn.nProtocolVersion < nMinProtocol) continue;
        if(fOnlyActive && !mn.IsEnabled()) continue;

        int64_t nScore = mn.CalculateScore(blockHash).GetCompact(false);

        vecApollonnodeScores.push_back(std::make_pair(nScore, &mn));
    }

    sort(vecApollonnodeScores.rbegin(), vecApollonnodeScores.rend(), CompareScoreMN());

    int rank = 0;
    BOOST_FOREACH (PAIRTYPE(int64_t, CApollonnode*)& s, vecApollonnodeScores){
        rank++;
        if(rank == nRank) {
            return s.second;
        }
    }

    return NULL;
}

void CApollonnodeMan::ProcessApollonnodeConnections()
{
    //we don't care about this for regtest
    if(Params().NetworkIDString() == CBaseChainParams::REGTEST) return;

    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes) {
        if(pnode->fApollonnode) {
            if(darkSendPool.pSubmittedToApollonnode != NULL && pnode->addr == darkSendPool.pSubmittedToApollonnode->addr) continue;
            // LogPrintf("Closing Apollonnode connection: peer=%d, addr=%s\n", pnode->id, pnode->addr.ToString());
            pnode->fDisconnect = true;
        }
    }
}

std::pair<CService, std::set<uint256> > CApollonnodeMan::PopScheduledMnbRequestConnection()
{
    LOCK(cs);
    if(listScheduledMnbRequestConnections.empty()) {
        return std::make_pair(CService(), std::set<uint256>());
    }

    std::set<uint256> setResult;

    listScheduledMnbRequestConnections.sort();
    std::pair<CService, uint256> pairFront = listScheduledMnbRequestConnections.front();

    // squash hashes from requests with the same CService as the first one into setResult
    std::list< std::pair<CService, uint256> >::iterator it = listScheduledMnbRequestConnections.begin();
    while(it != listScheduledMnbRequestConnections.end()) {
        if(pairFront.first == it->first) {
            setResult.insert(it->second);
            it = listScheduledMnbRequestConnections.erase(it);
        } else {
            // since list is sorted now, we can be sure that there is no more hashes left
            // to ask for from this addr
            break;
        }
    }
    return std::make_pair(pairFront.first, setResult);
}


void CApollonnodeMan::ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{

//    LogPrint("apollonnode", "CApollonnodeMan::ProcessMessage, strCommand=%s\n", strCommand);
    if(fLiteMode) return; // disable all Apollon specific functionality
    if(!apollonnodeSync.IsBlockchainSynced()) return;

    if (strCommand == NetMsgType::MNANNOUNCE) { //Apollonnode Broadcast
        CApollonnodeBroadcast mnb;
        vRecv >> mnb;

        pfrom->setAskFor.erase(mnb.GetHash());

        LogPrintf("MNANNOUNCE -- Apollonnode announce, apollonnode=%s\n", mnb.vin.prevout.ToStringShort());

        int nDos = 0;

        if (CheckMnbAndUpdateApollonnodeList(pfrom, mnb, nDos)) {
            // use announced Apollonnode as a peer
            addrman.Add(CAddress(mnb.addr, NODE_NETWORK), pfrom->addr, 2*60*60);
        } else if(nDos > 0) {
            Misbehaving(pfrom->GetId(), nDos);
        }

        if(fApollonnodesAdded) {
            NotifyApollonnodeUpdates();
        }
    } else if (strCommand == NetMsgType::MNPING) { //Apollonnode Ping

        CApollonnodePing mnp;
        vRecv >> mnp;

        uint256 nHash = mnp.GetHash();

        pfrom->setAskFor.erase(nHash);

        LogPrint("apollonnode", "MNPING -- Apollonnode ping, apollonnode=%s\n", mnp.vin.prevout.ToStringShort());

        // Need LOCK2 here to ensure consistent locking order because the CheckAndUpdate call below locks cs_main
        LOCK2(cs_main, cs);

        if(mapSeenApollonnodePing.count(nHash)) return; //seen
        mapSeenApollonnodePing.insert(std::make_pair(nHash, mnp));

        LogPrint("apollonnode", "MNPING -- Apollonnode ping, apollonnode=%s new\n", mnp.vin.prevout.ToStringShort());

        // see if we have this Apollonnode
        CApollonnode* pmn = mnodeman.Find(mnp.vin);

        // too late, new MNANNOUNCE is required
        if(pmn && pmn->IsNewStartRequired()) return;

        int nDos = 0;
        if(mnp.CheckAndUpdate(pmn, false, nDos)) return;

        if(nDos > 0) {
            // if anything significant failed, mark that node
            Misbehaving(pfrom->GetId(), nDos);
        } else if(pmn != NULL) {
            // nothing significant failed, mn is a known one too
            return;
        }

        // something significant is broken or mn is unknown,
        // we might have to ask for a apollonnode entry once
        AskForMN(pfrom, mnp.vin);

    } else if (strCommand == NetMsgType::DSEG) { //Get Apollonnode list or specific entry
        // Ignore such requests until we are fully synced.
        // We could start processing this after apollonnode list is synced
        // but this is a heavy one so it's better to finish sync first.
        if (!apollonnodeSync.IsSynced()) return;

        CTxIn vin;
        vRecv >> vin;

        LogPrint("apollonnode", "DSEG -- Apollonnode list, apollonnode=%s\n", vin.prevout.ToStringShort());

        LOCK(cs);

        if(vin == CTxIn()) { //only should ask for this once
            //local network
            bool isLocal = (pfrom->addr.IsRFC1918() || pfrom->addr.IsLocal());

            if(!isLocal && Params().NetworkIDString() == CBaseChainParams::MAIN) {
                std::map<CNetAddr, int64_t>::iterator i = mAskedUsForApollonnodeList.find(pfrom->addr);
                if (i != mAskedUsForApollonnodeList.end()){
                    int64_t t = (*i).second;
                    if (GetTime() < t) {
                        Misbehaving(pfrom->GetId(), 34);
                        LogPrintf("DSEG -- peer already asked me for the list, peer=%d\n", pfrom->id);
                        return;
                    }
                }
                int64_t askAgain = GetTime() + DSEG_UPDATE_SECONDS;
                mAskedUsForApollonnodeList[pfrom->addr] = askAgain;
            }
        } //else, asking for a specific node which is ok

        int nInvCount = 0;

        BOOST_FOREACH(CApollonnode& mn, vApollonnodes) {
            if (vin != CTxIn() && vin != mn.vin) continue; // asked for specific vin but we are not there yet
            if (mn.addr.IsRFC1918() || mn.addr.IsLocal()) continue; // do not send local network apollonnode
            if (mn.IsUpdateRequired()) continue; // do not send outdated apollonnodes

            LogPrint("apollonnode", "DSEG -- Sending Apollonnode entry: apollonnode=%s  addr=%s\n", mn.vin.prevout.ToStringShort(), mn.addr.ToString());
            CApollonnodeBroadcast mnb = CApollonnodeBroadcast(mn);
            uint256 hash = mnb.GetHash();
            pfrom->PushInventory(CInv(MSG_APOLLONNODE_ANNOUNCE, hash));
            pfrom->PushInventory(CInv(MSG_APOLLONNODE_PING, mn.lastPing.GetHash()));
            nInvCount++;

            if (!mapSeenApollonnodeBroadcast.count(hash)) {
                mapSeenApollonnodeBroadcast.insert(std::make_pair(hash, std::make_pair(GetTime(), mnb)));
            }

            if (vin == mn.vin) {
                LogPrintf("DSEG -- Sent 1 Apollonnode inv to peer %d\n", pfrom->id);
                return;
            }
        }

        if(vin == CTxIn()) {
            pfrom->PushMessage(NetMsgType::SYNCSTATUSCOUNT, APOLLONNODE_SYNC_LIST, nInvCount);
            LogPrintf("DSEG -- Sent %d Apollonnode invs to peer %d\n", nInvCount, pfrom->id);
            return;
        }
        // smth weird happen - someone asked us for vin we have no idea about?
        LogPrint("apollonnode", "DSEG -- No invs sent to peer %d\n", pfrom->id);

    } else if (strCommand == NetMsgType::MNVERIFY) { // Apollonnode Verify

        // Need LOCK2 here to ensure consistent locking order because the all functions below call GetBlockHash which locks cs_main
        LOCK2(cs_main, cs);

        CApollonnodeVerification mnv;
        vRecv >> mnv;

        if(mnv.vchSig1.empty()) {
            // CASE 1: someone asked me to verify myself /IP we are using/
            SendVerifyReply(pfrom, mnv);
        } else if (mnv.vchSig2.empty()) {
            // CASE 2: we _probably_ got verification we requested from some apollonnode
            ProcessVerifyReply(pfrom, mnv);
        } else {
            // CASE 3: we _probably_ got verification broadcast signed by some apollonnode which verified another one
            ProcessVerifyBroadcast(pfrom, mnv);
        }
    }
}

// Verification of apollonnodes via unique direct requests.

void CApollonnodeMan::DoFullVerificationStep()
{
    if(activeApollonnode.vin == CTxIn()) return;
    if(!apollonnodeSync.IsSynced()) return;

    std::vector<std::pair<int, CApollonnode> > vecApollonnodeRanks = GetApollonnodeRanks(pCurrentBlockApollon->nHeight - 1, MIN_POSE_PROTO_VERSION);

    // Need LOCK2 here to ensure consistent locking order because the SendVerifyRequest call below locks cs_main
    // through GetHeight() signal in ConnectNode
    LOCK2(cs_main, cs);

    int nCount = 0;

    int nMyRank = -1;
    int nRanksTotal = (int)vecApollonnodeRanks.size();

    // send verify requests only if we are in top MAX_POSE_RANK
    std::vector<std::pair<int, CApollonnode> >::iterator it = vecApollonnodeRanks.begin();
    while(it != vecApollonnodeRanks.end()) {
        if(it->first > MAX_POSE_RANK) {
            LogPrint("apollonnode", "CApollonnodeMan::DoFullVerificationStep -- Must be in top %d to send verify request\n",
                        (int)MAX_POSE_RANK);
            return;
        }
        if(it->second.vin == activeApollonnode.vin) {
            nMyRank = it->first;
            LogPrint("apollonnode", "CApollonnodeMan::DoFullVerificationStep -- Found self at rank %d/%d, verifying up to %d apollonnodes\n",
                        nMyRank, nRanksTotal, (int)MAX_POSE_CONNECTIONS);
            break;
        }
        ++it;
    }

    // edge case: list is too short and this apollonnode is not enabled
    if(nMyRank == -1) return;

    // send verify requests to up to MAX_POSE_CONNECTIONS apollonnodes
    // starting from MAX_POSE_RANK + nMyRank and using MAX_POSE_CONNECTIONS as a step
    int nOffset = MAX_POSE_RANK + nMyRank - 1;
    if(nOffset >= (int)vecApollonnodeRanks.size()) return;

    std::vector<CApollonnode*> vSortedByAddr;
    BOOST_FOREACH(CApollonnode& mn, vApollonnodes) {
        vSortedByAddr.push_back(&mn);
    }

    sort(vSortedByAddr.begin(), vSortedByAddr.end(), CompareByAddr());

    it = vecApollonnodeRanks.begin() + nOffset;
    while(it != vecApollonnodeRanks.end()) {
        if(it->second.IsPoSeVerified() || it->second.IsPoSeBanned()) {
            LogPrint("apollonnode", "CApollonnodeMan::DoFullVerificationStep -- Already %s%s%s apollonnode %s address %s, skipping...\n",
                        it->second.IsPoSeVerified() ? "verified" : "",
                        it->second.IsPoSeVerified() && it->second.IsPoSeBanned() ? " and " : "",
                        it->second.IsPoSeBanned() ? "banned" : "",
                        it->second.vin.prevout.ToStringShort(), it->second.addr.ToString());
            nOffset += MAX_POSE_CONNECTIONS;
            if(nOffset >= (int)vecApollonnodeRanks.size()) break;
            it += MAX_POSE_CONNECTIONS;
            continue;
        }
        LogPrint("apollonnode", "CApollonnodeMan::DoFullVerificationStep -- Verifying apollonnode %s rank %d/%d address %s\n",
                    it->second.vin.prevout.ToStringShort(), it->first, nRanksTotal, it->second.addr.ToString());
        if(SendVerifyRequest(CAddress(it->second.addr, NODE_NETWORK), vSortedByAddr)) {
            nCount++;
            if(nCount >= MAX_POSE_CONNECTIONS) break;
        }
        nOffset += MAX_POSE_CONNECTIONS;
        if(nOffset >= (int)vecApollonnodeRanks.size()) break;
        it += MAX_POSE_CONNECTIONS;
    }

    LogPrint("apollonnode", "CApollonnodeMan::DoFullVerificationStep -- Sent verification requests to %d apollonnodes\n", nCount);
}

// This function tries to find apollonnodes with the same addr,
// find a verified one and ban all the other. If there are many nodes
// with the same addr but none of them is verified yet, then none of them are banned.
// It could take many times to run this before most of the duplicate nodes are banned.

void CApollonnodeMan::CheckSameAddr()
{
    if(!apollonnodeSync.IsSynced() || vApollonnodes.empty()) return;

    std::vector<CApollonnode*> vBan;
    std::vector<CApollonnode*> vSortedByAddr;

    {
        LOCK(cs);

        CApollonnode* pprevApollonnode = NULL;
        CApollonnode* pverifiedApollonnode = NULL;

        BOOST_FOREACH(CApollonnode& mn, vApollonnodes) {
            vSortedByAddr.push_back(&mn);
        }

        sort(vSortedByAddr.begin(), vSortedByAddr.end(), CompareByAddr());

        BOOST_FOREACH(CApollonnode* pmn, vSortedByAddr) {
            // check only (pre)enabled apollonnodes
            if(!pmn->IsEnabled() && !pmn->IsPreEnabled()) continue;
            // initial step
            if(!pprevApollonnode) {
                pprevApollonnode = pmn;
                pverifiedApollonnode = pmn->IsPoSeVerified() ? pmn : NULL;
                continue;
            }
            // second+ step
            if(pmn->addr == pprevApollonnode->addr) {
                if(pverifiedApollonnode) {
                    // another apollonnode with the same ip is verified, ban this one
                    vBan.push_back(pmn);
                } else if(pmn->IsPoSeVerified()) {
                    // this apollonnode with the same ip is verified, ban previous one
                    vBan.push_back(pprevApollonnode);
                    // and keep a reference to be able to ban following apollonnodes with the same ip
                    pverifiedApollonnode = pmn;
                }
            } else {
                pverifiedApollonnode = pmn->IsPoSeVerified() ? pmn : NULL;
            }
            pprevApollonnode = pmn;
        }
    }

    // ban duplicates
    BOOST_FOREACH(CApollonnode* pmn, vBan) {
        LogPrintf("CApollonnodeMan::CheckSameAddr -- increasing PoSe ban score for apollonnode %s\n", pmn->vin.prevout.ToStringShort());
        pmn->IncreasePoSeBanScore();
    }
}

bool CApollonnodeMan::SendVerifyRequest(const CAddress& addr, const std::vector<CApollonnode*>& vSortedByAddr)
{
    if(netfulfilledman.HasFulfilledRequest(addr, strprintf("%s", NetMsgType::MNVERIFY)+"-request")) {
        // we already asked for verification, not a good idea to do this too often, skip it
        LogPrint("apollonnode", "CApollonnodeMan::SendVerifyRequest -- too many requests, skipping... addr=%s\n", addr.ToString());
        return false;
    }

    CNode* pnode = ConnectNode(addr, NULL, false, true);
    if(pnode == NULL) {
        LogPrintf("CApollonnodeMan::SendVerifyRequest -- can't connect to node to verify it, addr=%s\n", addr.ToString());
        return false;
    }

    netfulfilledman.AddFulfilledRequest(addr, strprintf("%s", NetMsgType::MNVERIFY)+"-request");
    // use random nonce, store it and require node to reply with correct one later
    CApollonnodeVerification mnv(addr, GetRandInt(999999), pCurrentBlockApollon->nHeight - 1);
    mWeAskedForVerification[addr] = mnv;
    LogPrintf("CApollonnodeMan::SendVerifyRequest -- verifying node using nonce %d addr=%s\n", mnv.nonce, addr.ToString());
    pnode->PushMessage(NetMsgType::MNVERIFY, mnv);

    return true;
}

void CApollonnodeMan::SendVerifyReply(CNode* pnode, CApollonnodeVerification& mnv)
{
    // only apollonnodes can sign this, why would someone ask regular node?
    if(!fApollonNode) {
        // do not ban, malicious node might be using my IP
        // and trying to confuse the node which tries to verify it
        return;
    }

    if(netfulfilledman.HasFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-reply")) {
//        // peer should not ask us that often
        LogPrintf("ApollonnodeMan::SendVerifyReply -- ERROR: peer already asked me recently, peer=%d\n", pnode->id);
        Misbehaving(pnode->id, 20);
        return;
    }

    uint256 blockHash;
    if(!GetBlockHash(blockHash, mnv.nBlockHeight)) {
        LogPrintf("ApollonnodeMan::SendVerifyReply -- can't get block hash for unknown block height %d, peer=%d\n", mnv.nBlockHeight, pnode->id);
        return;
    }

    std::string strMessage = strprintf("%s%d%s", activeApollonnode.service.ToString(), mnv.nonce, blockHash.ToString());

    if(!darkSendSigner.SignMessage(strMessage, mnv.vchSig1, activeApollonnode.keyApollonnode)) {
        LogPrintf("ApollonnodeMan::SendVerifyReply -- SignMessage() failed\n");
        return;
    }

    std::string strError;

    if(!darkSendSigner.VerifyMessage(activeApollonnode.pubKeyApollonnode, mnv.vchSig1, strMessage, strError)) {
        LogPrintf("ApollonnodeMan::SendVerifyReply -- VerifyMessage() failed, error: %s\n", strError);
        return;
    }

    pnode->PushMessage(NetMsgType::MNVERIFY, mnv);
    netfulfilledman.AddFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-reply");
}

void CApollonnodeMan::ProcessVerifyReply(CNode* pnode, CApollonnodeVerification& mnv)
{
    std::string strError;

    // did we even ask for it? if that's the case we should have matching fulfilled request
    if(!netfulfilledman.HasFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-request")) {
        LogPrintf("CApollonnodeMan::ProcessVerifyReply -- ERROR: we didn't ask for verification of %s, peer=%d\n", pnode->addr.ToString(), pnode->id);
        Misbehaving(pnode->id, 20);
        return;
    }

    // Received nonce for a known address must match the one we sent
    if(mWeAskedForVerification[pnode->addr].nonce != mnv.nonce) {
        LogPrintf("CApollonnodeMan::ProcessVerifyReply -- ERROR: wrong nounce: requested=%d, received=%d, peer=%d\n",
                    mWeAskedForVerification[pnode->addr].nonce, mnv.nonce, pnode->id);
        Misbehaving(pnode->id, 20);
        return;
    }

    // Received nBlockHeight for a known address must match the one we sent
    if(mWeAskedForVerification[pnode->addr].nBlockHeight != mnv.nBlockHeight) {
        LogPrintf("CApollonnodeMan::ProcessVerifyReply -- ERROR: wrong nBlockHeight: requested=%d, received=%d, peer=%d\n",
                    mWeAskedForVerification[pnode->addr].nBlockHeight, mnv.nBlockHeight, pnode->id);
        Misbehaving(pnode->id, 20);
        return;
    }

    uint256 blockHash;
    if(!GetBlockHash(blockHash, mnv.nBlockHeight)) {
        // this shouldn't happen...
        LogPrintf("ApollonnodeMan::ProcessVerifyReply -- can't get block hash for unknown block height %d, peer=%d\n", mnv.nBlockHeight, pnode->id);
        return;
    }

//    // we already verified this address, why node is spamming?
    if(netfulfilledman.HasFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-done")) {
        LogPrintf("CApollonnodeMan::ProcessVerifyReply -- ERROR: already verified %s recently\n", pnode->addr.ToString());
        Misbehaving(pnode->id, 20);
        return;
    }

    {
        LOCK(cs);

        CApollonnode* prealApollonnode = NULL;
        std::vector<CApollonnode*> vpApollonnodesToBan;
        std::vector<CApollonnode>::iterator it = vApollonnodes.begin();
        std::string strMessage1 = strprintf("%s%d%s", pnode->addr.ToString(), mnv.nonce, blockHash.ToString());
        while(it != vApollonnodes.end()) {
            if(CAddress(it->addr, NODE_NETWORK) == pnode->addr) {
                if(darkSendSigner.VerifyMessage(it->pubKeyApollonnode, mnv.vchSig1, strMessage1, strError)) {
                    // found it!
                    prealApollonnode = &(*it);
                    if(!it->IsPoSeVerified()) {
                        it->DecreasePoSeBanScore();
                    }
                    netfulfilledman.AddFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-done");

                    // we can only broadcast it if we are an activated apollonnode
                    if(activeApollonnode.vin == CTxIn()) continue;
                    // update ...
                    mnv.addr = it->addr;
                    mnv.vin1 = it->vin;
                    mnv.vin2 = activeApollonnode.vin;
                    std::string strMessage2 = strprintf("%s%d%s%s%s", mnv.addr.ToString(), mnv.nonce, blockHash.ToString(),
                                            mnv.vin1.prevout.ToStringShort(), mnv.vin2.prevout.ToStringShort());
                    // ... and sign it
                    if(!darkSendSigner.SignMessage(strMessage2, mnv.vchSig2, activeApollonnode.keyApollonnode)) {
                        LogPrintf("ApollonnodeMan::ProcessVerifyReply -- SignMessage() failed\n");
                        return;
                    }

                    std::string strError;

                    if(!darkSendSigner.VerifyMessage(activeApollonnode.pubKeyApollonnode, mnv.vchSig2, strMessage2, strError)) {
                        LogPrintf("ApollonnodeMan::ProcessVerifyReply -- VerifyMessage() failed, error: %s\n", strError);
                        return;
                    }

                    mWeAskedForVerification[pnode->addr] = mnv;
                    mnv.Relay();

                } else {
                    vpApollonnodesToBan.push_back(&(*it));
                }
            }
            ++it;
        }
        // no real apollonnode found?...
        if(!prealApollonnode) {
            // this should never be the case normally,
            // only if someone is trying to game the system in some way or smth like that
            LogPrintf("CApollonnodeMan::ProcessVerifyReply -- ERROR: no real apollonnode found for addr %s\n", pnode->addr.ToString());
            Misbehaving(pnode->id, 20);
            return;
        }
        LogPrintf("CApollonnodeMan::ProcessVerifyReply -- verified real apollonnode %s for addr %s\n",
                    prealApollonnode->vin.prevout.ToStringShort(), pnode->addr.ToString());
        // increase ban score for everyone else
        BOOST_FOREACH(CApollonnode* pmn, vpApollonnodesToBan) {
            pmn->IncreasePoSeBanScore();
            LogPrint("apollonnode", "CApollonnodeMan::ProcessVerifyBroadcast -- increased PoSe ban score for %s addr %s, new score %d\n",
                        prealApollonnode->vin.prevout.ToStringShort(), pnode->addr.ToString(), pmn->nPoSeBanScore);
        }
        LogPrintf("CApollonnodeMan::ProcessVerifyBroadcast -- PoSe score increased for %d fake apollonnodes, addr %s\n",
                    (int)vpApollonnodesToBan.size(), pnode->addr.ToString());
    }
}

void CApollonnodeMan::ProcessVerifyBroadcast(CNode* pnode, const CApollonnodeVerification& mnv)
{
    std::string strError;

    if(mapSeenApollonnodeVerification.find(mnv.GetHash()) != mapSeenApollonnodeVerification.end()) {
        // we already have one
        return;
    }
    mapSeenApollonnodeVerification[mnv.GetHash()] = mnv;

    // we don't care about history
    if(mnv.nBlockHeight < pCurrentBlockApollon->nHeight - MAX_POSE_BLOCKS) {
        LogPrint("apollonnode", "ApollonnodeMan::ProcessVerifyBroadcast -- Outdated: current block %d, verification block %d, peer=%d\n",
                    pCurrentBlockApollon->nHeight, mnv.nBlockHeight, pnode->id);
        return;
    }

    if(mnv.vin1.prevout == mnv.vin2.prevout) {
        LogPrint("apollonnode", "ApollonnodeMan::ProcessVerifyBroadcast -- ERROR: same vins %s, peer=%d\n",
                    mnv.vin1.prevout.ToStringShort(), pnode->id);
        // that was NOT a good idea to cheat and verify itself,
        // ban the node we received such message from
        Misbehaving(pnode->id, 100);
        return;
    }

    uint256 blockHash;
    if(!GetBlockHash(blockHash, mnv.nBlockHeight)) {
        // this shouldn't happen...
        LogPrintf("ApollonnodeMan::ProcessVerifyBroadcast -- Can't get block hash for unknown block height %d, peer=%d\n", mnv.nBlockHeight, pnode->id);
        return;
    }

    int nRank = GetApollonnodeRank(mnv.vin2, mnv.nBlockHeight, MIN_POSE_PROTO_VERSION);

    if (nRank == -1) {
        LogPrint("apollonnode", "CApollonnodeMan::ProcessVerifyBroadcast -- Can't calculate rank for apollonnode %s\n",
                    mnv.vin2.prevout.ToStringShort());
        return;
    }

    if(nRank > MAX_POSE_RANK) {
        LogPrint("apollonnode", "CApollonnodeMan::ProcessVerifyBroadcast -- Mastrernode %s is not in top %d, current rank %d, peer=%d\n",
                    mnv.vin2.prevout.ToStringShort(), (int)MAX_POSE_RANK, nRank, pnode->id);
        return;
    }

    {
        LOCK(cs);

        std::string strMessage1 = strprintf("%s%d%s", mnv.addr.ToString(), mnv.nonce, blockHash.ToString());
        std::string strMessage2 = strprintf("%s%d%s%s%s", mnv.addr.ToString(), mnv.nonce, blockHash.ToString(),
                                mnv.vin1.prevout.ToStringShort(), mnv.vin2.prevout.ToStringShort());

        CApollonnode* pmn1 = Find(mnv.vin1);
        if(!pmn1) {
            LogPrintf("CApollonnodeMan::ProcessVerifyBroadcast -- can't find apollonnode1 %s\n", mnv.vin1.prevout.ToStringShort());
            return;
        }

        CApollonnode* pmn2 = Find(mnv.vin2);
        if(!pmn2) {
            LogPrintf("CApollonnodeMan::ProcessVerifyBroadcast -- can't find apollonnode2 %s\n", mnv.vin2.prevout.ToStringShort());
            return;
        }

        if(pmn1->addr != mnv.addr) {
            LogPrintf("CApollonnodeMan::ProcessVerifyBroadcast -- addr %s do not match %s\n", mnv.addr.ToString(), pnode->addr.ToString());
            return;
        }

        if(darkSendSigner.VerifyMessage(pmn1->pubKeyApollonnode, mnv.vchSig1, strMessage1, strError)) {
            LogPrintf("ApollonnodeMan::ProcessVerifyBroadcast -- VerifyMessage() for apollonnode1 failed, error: %s\n", strError);
            return;
        }

        if(darkSendSigner.VerifyMessage(pmn2->pubKeyApollonnode, mnv.vchSig2, strMessage2, strError)) {
            LogPrintf("ApollonnodeMan::ProcessVerifyBroadcast -- VerifyMessage() for apollonnode2 failed, error: %s\n", strError);
            return;
        }

        if(!pmn1->IsPoSeVerified()) {
            pmn1->DecreasePoSeBanScore();
        }
        mnv.Relay();

        LogPrintf("CApollonnodeMan::ProcessVerifyBroadcast -- verified apollonnode %s for addr %s\n",
                    pmn1->vin.prevout.ToStringShort(), pnode->addr.ToString());

        // increase ban score for everyone else with the same addr
        int nCount = 0;
        BOOST_FOREACH(CApollonnode& mn, vApollonnodes) {
            if(mn.addr != mnv.addr || mn.vin.prevout == mnv.vin1.prevout) continue;
            mn.IncreasePoSeBanScore();
            nCount++;
            LogPrint("apollonnode", "CApollonnodeMan::ProcessVerifyBroadcast -- increased PoSe ban score for %s addr %s, new score %d\n",
                        mn.vin.prevout.ToStringShort(), mn.addr.ToString(), mn.nPoSeBanScore);
        }
        LogPrintf("CApollonnodeMan::ProcessVerifyBroadcast -- PoSe score incresed for %d fake apollonnodes, addr %s\n",
                    nCount, pnode->addr.ToString());
    }
}

std::string CApollonnodeMan::ToString() const
{
    std::ostringstream info;

    info << "Apollonnodes: " << (int)vApollonnodes.size() <<
            ", peers who asked us for Apollonnode list: " << (int)mAskedUsForApollonnodeList.size() <<
            ", peers we asked for Apollonnode list: " << (int)mWeAskedForApollonnodeList.size() <<
            ", entries in Apollonnode list we asked for: " << (int)mWeAskedForApollonnodeListEntry.size() <<
            ", apollonnode apollon size: " << apollonApollonnodes.GetSize() <<
            ", nDsqCount: " << (int)nDsqCount;

    return info.str();
}

void CApollonnodeMan::UpdateApollonnodeList(CApollonnodeBroadcast mnb)
{
    try {
        LogPrintf("CApollonnodeMan::UpdateApollonnodeList\n");
        LOCK2(cs_main, cs);
        mapSeenApollonnodePing.insert(std::make_pair(mnb.lastPing.GetHash(), mnb.lastPing));
        mapSeenApollonnodeBroadcast.insert(std::make_pair(mnb.GetHash(), std::make_pair(GetTime(), mnb)));

        LogPrintf("CApollonnodeMan::UpdateApollonnodeList -- apollonnode=%s  addr=%s\n", mnb.vin.prevout.ToStringShort(), mnb.addr.ToString());

        CApollonnode *pmn = Find(mnb.vin);
        if (pmn == NULL) {
            CApollonnode mn(mnb);
            if (Add(mn)) {
                apollonnodeSync.AddedApollonnodeList();
                GetMainSignals().UpdatedApollonnode(mn);
            }
        } else {
            CApollonnodeBroadcast mnbOld = mapSeenApollonnodeBroadcast[CApollonnodeBroadcast(*pmn).GetHash()].second;
            if (pmn->UpdateFromNewBroadcast(mnb)) {
                apollonnodeSync.AddedApollonnodeList();
                GetMainSignals().UpdatedApollonnode(*pmn);
                mapSeenApollonnodeBroadcast.erase(mnbOld.GetHash());
            }
        }
    } catch (const std::exception &e) {
        PrintExceptionContinue(&e, "UpdateApollonnodeList");
    }
}

bool CApollonnodeMan::CheckMnbAndUpdateApollonnodeList(CNode* pfrom, CApollonnodeBroadcast mnb, int& nDos)
{
    // Need LOCK2 here to ensure consistent locking order because the SimpleCheck call below locks cs_main
    LOCK(cs_main);

    {
        LOCK(cs);
        nDos = 0;
        LogPrint("apollonnode", "CApollonnodeMan::CheckMnbAndUpdateApollonnodeList -- apollonnode=%s\n", mnb.vin.prevout.ToStringShort());

        uint256 hash = mnb.GetHash();
        if (mapSeenApollonnodeBroadcast.count(hash) && !mnb.fRecovery) { //seen
            LogPrint("apollonnode", "CApollonnodeMan::CheckMnbAndUpdateApollonnodeList -- apollonnode=%s seen\n", mnb.vin.prevout.ToStringShort());
            // less then 2 pings left before this MN goes into non-recoverable state, bump sync timeout
            if (GetTime() - mapSeenApollonnodeBroadcast[hash].first > APOLLONNODE_NEW_START_REQUIRED_SECONDS - APOLLONNODE_MIN_MNP_SECONDS * 2) {
                LogPrint("apollonnode", "CApollonnodeMan::CheckMnbAndUpdateApollonnodeList -- apollonnode=%s seen update\n", mnb.vin.prevout.ToStringShort());
                mapSeenApollonnodeBroadcast[hash].first = GetTime();
                apollonnodeSync.AddedApollonnodeList();
                GetMainSignals().UpdatedApollonnode(mnb);
            }
            // did we ask this node for it?
            if (pfrom && IsMnbRecoveryRequested(hash) && GetTime() < mMnbRecoveryRequests[hash].first) {
                LogPrint("apollonnode", "CApollonnodeMan::CheckMnbAndUpdateApollonnodeList -- mnb=%s seen request\n", hash.ToString());
                if (mMnbRecoveryRequests[hash].second.count(pfrom->addr)) {
                    LogPrint("apollonnode", "CApollonnodeMan::CheckMnbAndUpdateApollonnodeList -- mnb=%s seen request, addr=%s\n", hash.ToString(), pfrom->addr.ToString());
                    // do not allow node to send same mnb multiple times in recovery mode
                    mMnbRecoveryRequests[hash].second.erase(pfrom->addr);
                    // does it have newer lastPing?
                    if (mnb.lastPing.sigTime > mapSeenApollonnodeBroadcast[hash].second.lastPing.sigTime) {
                        // simulate Check
                        CApollonnode mnTemp = CApollonnode(mnb);
                        mnTemp.Check();
                        LogPrint("apollonnode", "CApollonnodeMan::CheckMnbAndUpdateApollonnodeList -- mnb=%s seen request, addr=%s, better lastPing: %d min ago, projected mn state: %s\n", hash.ToString(), pfrom->addr.ToString(), (GetTime() - mnb.lastPing.sigTime) / 60, mnTemp.GetStateString());
                        if (mnTemp.IsValidStateForAutoStart(mnTemp.nActiveState)) {
                            // this node thinks it's a good one
                            LogPrint("apollonnode", "CApollonnodeMan::CheckMnbAndUpdateApollonnodeList -- apollonnode=%s seen good\n", mnb.vin.prevout.ToStringShort());
                            mMnbRecoveryGoodReplies[hash].push_back(mnb);
                        }
                    }
                }
            }
            return true;
        }
        mapSeenApollonnodeBroadcast.insert(std::make_pair(hash, std::make_pair(GetTime(), mnb)));

        LogPrint("apollonnode", "CApollonnodeMan::CheckMnbAndUpdateApollonnodeList -- apollonnode=%s new\n", mnb.vin.prevout.ToStringShort());

        if (!mnb.SimpleCheck(nDos)) {
            LogPrint("apollonnode", "CApollonnodeMan::CheckMnbAndUpdateApollonnodeList -- SimpleCheck() failed, apollonnode=%s\n", mnb.vin.prevout.ToStringShort());
            return false;
        }

        // search Apollonnode list
        CApollonnode *pmn = Find(mnb.vin);
        if (pmn) {
            CApollonnodeBroadcast mnbOld = mapSeenApollonnodeBroadcast[CApollonnodeBroadcast(*pmn).GetHash()].second;
            if (!mnb.Update(pmn, nDos)) {
                LogPrint("apollonnode", "CApollonnodeMan::CheckMnbAndUpdateApollonnodeList -- Update() failed, apollonnode=%s\n", mnb.vin.prevout.ToStringShort());
                return false;
            }
            if (hash != mnbOld.GetHash()) {
                mapSeenApollonnodeBroadcast.erase(mnbOld.GetHash());
            }
        }
    } // end of LOCK(cs);

    if(mnb.CheckOutpoint(nDos)) {
        if(Add(mnb)){
            GetMainSignals().UpdatedApollonnode(mnb);  
        }
        apollonnodeSync.AddedApollonnodeList();
        // if it matches our Apollonnode privkey...
        if(fApollonNode && mnb.pubKeyApollonnode == activeApollonnode.pubKeyApollonnode) {
            mnb.nPoSeBanScore = -APOLLONNODE_POSE_BAN_MAX_SCORE;
            if(mnb.nProtocolVersion == PROTOCOL_VERSION) {
                // ... and PROTOCOL_VERSION, then we've been remotely activated ...
                LogPrintf("CApollonnodeMan::CheckMnbAndUpdateApollonnodeList -- Got NEW Apollonnode entry: apollonnode=%s  sigTime=%lld  addr=%s\n",
                            mnb.vin.prevout.ToStringShort(), mnb.sigTime, mnb.addr.ToString());
                activeApollonnode.ManageState();
            } else {
                // ... otherwise we need to reactivate our node, do not add it to the list and do not relay
                // but also do not ban the node we get this message from
                LogPrintf("CApollonnodeMan::CheckMnbAndUpdateApollonnodeList -- wrong PROTOCOL_VERSION, re-activate your MN: message nProtocolVersion=%d  PROTOCOL_VERSION=%d\n", mnb.nProtocolVersion, PROTOCOL_VERSION);
                return false;
            }
        }
        mnb.RelayApollonNode();
    } else {
        LogPrintf("CApollonnodeMan::CheckMnbAndUpdateApollonnodeList -- Rejected Apollonnode entry: %s  addr=%s\n", mnb.vin.prevout.ToStringShort(), mnb.addr.ToString());
        return false;
    }

    return true;
}

void CApollonnodeMan::UpdateLastPaid()
{
    LOCK(cs);
    if(fLiteMode) return;
    if(!pCurrentBlockApollon) {
        // LogPrintf("CApollonnodeMan::UpdateLastPaid, pCurrentBlockApollon=NULL\n");
        return;
    }

    static bool IsFirstRun = true;
    // Do full scan on first run or if we are not a apollonnode
    // (MNs should update this info on every block, so limited scan should be enough for them)
    int nMaxBlocksToScanBack = (IsFirstRun || !fApollonNode) ? mnpayments.GetStorageLimit() : LAST_PAID_SCAN_BLOCKS;

    LogPrint("mnpayments", "CApollonnodeMan::UpdateLastPaid -- nHeight=%d, nMaxBlocksToScanBack=%d, IsFirstRun=%s\n",
                             pCurrentBlockApollon->nHeight, nMaxBlocksToScanBack, IsFirstRun ? "true" : "false");

    BOOST_FOREACH(CApollonnode& mn, vApollonnodes) {
        mn.UpdateLastPaid(pCurrentBlockApollon, nMaxBlocksToScanBack);
    }

    // every time is like the first time if winners list is not synced
    IsFirstRun = !apollonnodeSync.IsWinnersListSynced();
}

void CApollonnodeMan::CheckAndRebuildApollonnodeApollon()
{
    LOCK(cs);

    if(GetTime() - nLastApollonRebuildTime < MIN_APOLLON_REBUILD_TIME) {
        return;
    }

    if(apollonApollonnodes.GetSize() <= MAX_EXPECTED_APOLLON_SIZE) {
        return;
    }

    if(apollonApollonnodes.GetSize() <= int(vApollonnodes.size())) {
        return;
    }

    apollonApollonnodesOld = apollonApollonnodes;
    apollonApollonnodes.Clear();
    for(size_t i = 0; i < vApollonnodes.size(); ++i) {
        apollonApollonnodes.AddApollonnodeVIN(vApollonnodes[i].vin);
    }

    fApollonRebuilt = true;
    nLastApollonRebuildTime = GetTime();
}

void CApollonnodeMan::UpdateWatchdogVoteTime(const CTxIn& vin)
{
    LOCK(cs);
    CApollonnode* pMN = Find(vin);
    if(!pMN)  {
        return;
    }
    pMN->UpdateWatchdogVoteTime();
    nLastWatchdogVoteTime = GetTime();
}

bool CApollonnodeMan::IsWatchdogActive()
{
    LOCK(cs);
    // Check if any apollonnodes have voted recently, otherwise return false
    return (GetTime() - nLastWatchdogVoteTime) <= APOLLONNODE_WATCHDOG_MAX_SECONDS;
}

void CApollonnodeMan::CheckApollonnode(const CTxIn& vin, bool fForce)
{
    LOCK(cs);
    CApollonnode* pMN = Find(vin);
    if(!pMN)  {
        return;
    }
    pMN->Check(fForce);
}

void CApollonnodeMan::CheckApollonnode(const CPubKey& pubKeyApollonnode, bool fForce)
{
    LOCK(cs);
    CApollonnode* pMN = Find(pubKeyApollonnode);
    if(!pMN)  {
        return;
    }
    pMN->Check(fForce);
}

int CApollonnodeMan::GetApollonnodeState(const CTxIn& vin)
{
    LOCK(cs);
    CApollonnode* pMN = Find(vin);
    if(!pMN)  {
        return CApollonnode::APOLLONNODE_NEW_START_REQUIRED;
    }
    return pMN->nActiveState;
}

int CApollonnodeMan::GetApollonnodeState(const CPubKey& pubKeyApollonnode)
{
    LOCK(cs);
    CApollonnode* pMN = Find(pubKeyApollonnode);
    if(!pMN)  {
        return CApollonnode::APOLLONNODE_NEW_START_REQUIRED;
    }
    return pMN->nActiveState;
}

bool CApollonnodeMan::IsApollonnodePingedWithin(const CTxIn& vin, int nSeconds, int64_t nTimeToCheckAt)
{
    LOCK(cs);
    CApollonnode* pMN = Find(vin);
    if(!pMN) {
        return false;
    }
    return pMN->IsPingedWithin(nSeconds, nTimeToCheckAt);
}

void CApollonnodeMan::SetApollonnodeLastPing(const CTxIn& vin, const CApollonnodePing& mnp)
{
    LOCK(cs);
    CApollonnode* pMN = Find(vin);
    if(!pMN)  {
        return;
    }
    pMN->SetLastPing(mnp);
    mapSeenApollonnodePing.insert(std::make_pair(mnp.GetHash(), mnp));

    CApollonnodeBroadcast mnb(*pMN);
    uint256 hash = mnb.GetHash();
    if(mapSeenApollonnodeBroadcast.count(hash)) {
        mapSeenApollonnodeBroadcast[hash].second.SetLastPing(mnp);
    }
}

void CApollonnodeMan::UpdatedBlockTip(const CBlockApollon *papollon)
{
    pCurrentBlockApollon = papollon;
    LogPrint("apollonnode", "CApollonnodeMan::UpdatedBlockTip -- pCurrentBlockApollon->nHeight=%d\n", pCurrentBlockApollon->nHeight);

    CheckSameAddr();
    
    if(fApollonNode) {
        // normal wallet does not need to update this every block, doing update on rpc call should be enough
        UpdateLastPaid();
    }
}

void CApollonnodeMan::NotifyApollonnodeUpdates()
{
    // Avoid double locking
    bool fApollonnodesAddedLocal = false;
    bool fApollonnodesRemovedLocal = false;
    {
        LOCK(cs);
        fApollonnodesAddedLocal = fApollonnodesAdded;
        fApollonnodesRemovedLocal = fApollonnodesRemoved;
    }

    if(fApollonnodesAddedLocal) {
//        governance.CheckApollonnodeOrphanObjects();
//        governance.CheckApollonnodeOrphanVotes();
    }
    if(fApollonnodesRemovedLocal) {
//        governance.UpdateCachesAndClean();
    }

    LOCK(cs);
    fApollonnodesAdded = false;
    fApollonnodesRemoved = false;
}
