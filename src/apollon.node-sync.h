// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef APOLLONNODE_SYNC_H
#define APOLLONNODE_SYNC_H

#include "chain.h"
#include "net.h"

#include <univalue.h>

class CApollonnodeSync;

static const int APOLLONNODE_SYNC_FAILED          = -1;
static const int APOLLONNODE_SYNC_INITIAL         = 0;
static const int APOLLONNODE_SYNC_SPORKS          = 1;
static const int APOLLONNODE_SYNC_LIST            = 2;
static const int APOLLONNODE_SYNC_MNW             = 3;
static const int APOLLONNODE_SYNC_FINISHED        = 999;

static const int APOLLONNODE_SYNC_TICK_SECONDS    = 6;
static const int APOLLONNODE_SYNC_TIMEOUT_SECONDS = 30; // our blocks are 2.5 minutes so 30 seconds should be fine

static const int APOLLONNODE_SYNC_ENOUGH_PEERS    = 3;

static bool fBlockchainSynced = false;

extern CApollonnodeSync apollonnodeSync;

//
// CApollonnodeSync : Sync apollonnode assets in stages
//

class CApollonnodeSync
{
private:
    // Keep track of current asset
    int nRequestedApollonnodeAssets;
    // Count peers we've requested the asset from
    int nRequestedApollonnodeAttempt;

    // Time when current apollonnode asset sync started
    int64_t nTimeAssetSyncStarted;

    // Last time when we received some apollonnode asset ...
    int64_t nTimeLastApollonnodeList;
    int64_t nTimeLastPaymentVote;
    int64_t nTimeLastGovernanceItem;
    // ... or failed
    int64_t nTimeLastFailure;

    // How many times we failed
    int nCountFailures;

    // Keep track of current block apollon
    const CBlockApollon *pCurrentBlockApollon;

    bool CheckNodeHeight(CNode* pnode, bool fDisconnectStuckNodes = false);
    void Fail();
    void ClearFulfilledRequests();

public:
    CApollonnodeSync() { Reset(); }

    void AddedApollonnodeList() { nTimeLastApollonnodeList = GetTime(); }
    void AddedPaymentVote() { nTimeLastPaymentVote = GetTime(); }
    void AddedGovernanceItem() { nTimeLastGovernanceItem = GetTime(); };

    void SendGovernanceSyncRequest(CNode* pnode);

    bool GetBlockchainSynced(bool fBlockAccepted = false);

    bool IsFailed() { return nRequestedApollonnodeAssets == APOLLONNODE_SYNC_FAILED; }
    bool IsBlockchainSynced(bool fBlockAccepted = false);
    bool IsApollonnodeListSynced() { return nRequestedApollonnodeAssets > APOLLONNODE_SYNC_LIST; }
    bool IsWinnersListSynced() { return nRequestedApollonnodeAssets > APOLLONNODE_SYNC_MNW; }
    bool IsSynced() { return nRequestedApollonnodeAssets == APOLLONNODE_SYNC_FINISHED; }

    int GetAssetID() { return nRequestedApollonnodeAssets; }
    int GetAttempt() { return nRequestedApollonnodeAttempt; }
    std::string GetAssetName();
    std::string GetSyncStatus();

    void Reset();
    void SwitchToNextAsset();

    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
    void ProcessTick();

    void UpdatedBlockTip(const CBlockApollon *papollon);
};

#endif
