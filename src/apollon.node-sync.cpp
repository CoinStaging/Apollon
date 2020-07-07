// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activeapollonnode.h"
#include "checkpoints.h"
#include "main.h"
#include "apollonnode.h"
#include "apollonnode-payments.h"
#include "apollonnode-sync.h"
#include "apollonnodeman.h"
#include "netfulfilledman.h"
#include "spork.h"
#include "util.h"
#include "validationinterface.h"

class CApollonnodeSync;

CApollonnodeSync apollonnodeSync;

bool CApollonnodeSync::CheckNodeHeight(CNode *pnode, bool fDisconnectStuckNodes) {
    CNodeStateStats stats;
    if (!GetNodeStateStats(pnode->id, stats) || stats.nCommonHeight == -1 || stats.nSyncHeight == -1) return false; // not enough info about this peer

    // Check blocks and headers, allow a small error margin of 1 block
    if (pCurrentBlockApollon->nHeight - 1 > stats.nCommonHeight) {
        // This peer probably stuck, don't sync any additional data from it
        if (fDisconnectStuckNodes) {
            // Disconnect to free this connection slot for another peer.
            pnode->fDisconnect = true;
            LogPrintf("CApollonnodeSync::CheckNodeHeight -- disconnecting from stuck peer, nHeight=%d, nCommonHeight=%d, peer=%d\n",
                      pCurrentBlockApollon->nHeight, stats.nCommonHeight, pnode->id);
        } else {
            LogPrintf("CApollonnodeSync::CheckNodeHeight -- skipping stuck peer, nHeight=%d, nCommonHeight=%d, peer=%d\n",
                      pCurrentBlockApollon->nHeight, stats.nCommonHeight, pnode->id);
        }
        return false;
    } else if (pCurrentBlockApollon->nHeight < stats.nSyncHeight - 1) {
        // This peer announced more headers than we have blocks currently
        LogPrint("apollonnode", "CApollonnodeSync::CheckNodeHeight -- skipping peer, who announced more headers than we have blocks currently, nHeight=%d, nSyncHeight=%d, peer=%d\n",
                  pCurrentBlockApollon->nHeight, stats.nSyncHeight, pnode->id);
        return false;
    }

    return true;
}

bool CApollonnodeSync::GetBlockchainSynced(bool fBlockAccepted){
    bool currentBlockchainSynced = fBlockchainSynced;
    IsBlockchainSynced(fBlockAccepted);
    if(currentBlockchainSynced != fBlockchainSynced){
        GetMainSignals().UpdateSyncStatus();
    }
    return fBlockchainSynced;
}

bool CApollonnodeSync::IsBlockchainSynced(bool fBlockAccepted) {
    static int64_t nTimeLastProcess = GetTime();
    static int nSkipped = 0;
    static bool fFirstBlockAccepted = false;

    // If the last call to this function was more than 60 minutes ago 
    // (client was in sleep mode) reset the sync process
    if (GetTime() - nTimeLastProcess > 60 * 60) {
        LogPrintf("CApollonnodeSync::IsBlockchainSynced time-check fBlockchainSynced=%s\n", 
                  fBlockchainSynced);
        Reset();
        fBlockchainSynced = false;
    }

    if (!pCurrentBlockApollon || !papollonBestHeader || fImporting || fReapollon) 
        return false;

    if (fBlockAccepted) {
        // This should be only triggered while we are still syncing.
        if (!IsSynced()) {
            // We are trying to download smth, reset blockchain sync status.
            fFirstBlockAccepted = true;
            fBlockchainSynced = false;
            nTimeLastProcess = GetTime();
            return false;
        }
    } else {
        // Dont skip on REGTEST to make the tests run faster.
        if(Params().NetworkIDString() != CBaseChainParams::REGTEST) {
            // skip if we already checked less than 1 tick ago.
            if (GetTime() - nTimeLastProcess < APOLLONNODE_SYNC_TICK_SECONDS) {
                nSkipped++;
                return fBlockchainSynced;
            }
        }
    }

    LogPrint("apollonnode-sync", 
             "CApollonnodeSync::IsBlockchainSynced -- state before check: %ssynced, skipped %d times\n", 
             fBlockchainSynced ? "" : "not ", 
             nSkipped);

    nTimeLastProcess = GetTime();
    nSkipped = 0;

    if (fBlockchainSynced){
        return true;
    }

    if (fCheckpointsEnabled && 
        pCurrentBlockApollon->nHeight < Checkpoints::GetTotalBlocksEstimate(Params().Checkpoints())) {
        
        return false;
    }

    std::vector < CNode * > vNodesCopy = CopyNodeVector();
    // We have enough peers and assume most of them are synced
    if (vNodesCopy.size() >= APOLLONNODE_SYNC_ENOUGH_PEERS) {
        // Check to see how many of our peers are (almost) at the same height as we are
        int nNodesAtSameHeight = 0;
        BOOST_FOREACH(CNode * pnode, vNodesCopy)
        {
            // Make sure this peer is presumably at the same height
            if (!CheckNodeHeight(pnode)) {
                continue;
            }
            nNodesAtSameHeight++;
            // if we have decent number of such peers, most likely we are synced now
            if (nNodesAtSameHeight >= APOLLONNODE_SYNC_ENOUGH_PEERS) {
                LogPrintf("CApollonnodeSync::IsBlockchainSynced -- found enough peers on the same height as we are, done\n");
                fBlockchainSynced = true;
                ReleaseNodeVector(vNodesCopy);
                return fBlockchainSynced;
            }
        }
    }
    ReleaseNodeVector(vNodesCopy);

    // wait for at least one new block to be accepted
    if (!fFirstBlockAccepted){ 
        fBlockchainSynced = false;
        return false;
    }

    // same as !IsInitialBlockDownload() but no cs_main needed here
    int64_t nMaxBlockTime = std::max(pCurrentBlockApollon->GetBlockTime(), papollonBestHeader->GetBlockTime());
    fBlockchainSynced = papollonBestHeader->nHeight - pCurrentBlockApollon->nHeight < 24 * 6 &&
                        GetTime() - nMaxBlockTime < Params().MaxTipAge();
    return fBlockchainSynced;
}

void CApollonnodeSync::Fail() {
    nTimeLastFailure = GetTime();
    nRequestedApollonnodeAssets = APOLLONNODE_SYNC_FAILED;
    GetMainSignals().UpdateSyncStatus();
}

void CApollonnodeSync::Reset() {
    nRequestedApollonnodeAssets = APOLLONNODE_SYNC_INITIAL;
    nRequestedApollonnodeAttempt = 0;
    nTimeAssetSyncStarted = GetTime();
    nTimeLastApollonnodeList = GetTime();
    nTimeLastPaymentVote = GetTime();
    nTimeLastGovernanceItem = GetTime();
    nTimeLastFailure = 0;
    nCountFailures = 0;
}

std::string CApollonnodeSync::GetAssetName() {
    switch (nRequestedApollonnodeAssets) {
        case (APOLLONNODE_SYNC_INITIAL):
            return "APOLLONNODE_SYNC_INITIAL";
        case (APOLLONNODE_SYNC_SPORKS):
            return "APOLLONNODE_SYNC_SPORKS";
        case (APOLLONNODE_SYNC_LIST):
            return "APOLLONNODE_SYNC_LIST";
        case (APOLLONNODE_SYNC_MNW):
            return "APOLLONNODE_SYNC_MNW";
        case (APOLLONNODE_SYNC_FAILED):
            return "APOLLONNODE_SYNC_FAILED";
        case APOLLONNODE_SYNC_FINISHED:
            return "APOLLONNODE_SYNC_FINISHED";
        default:
            return "UNKNOWN";
    }
}

void CApollonnodeSync::SwitchToNextAsset() {
    switch (nRequestedApollonnodeAssets) {
        case (APOLLONNODE_SYNC_FAILED):
            throw std::runtime_error("Can't switch to next asset from failed, should use Reset() first!");
            break;
        case (APOLLONNODE_SYNC_INITIAL):
            ClearFulfilledRequests();
            nRequestedApollonnodeAssets = APOLLONNODE_SYNC_SPORKS;
            LogPrintf("CApollonnodeSync::SwitchToNextAsset -- Starting %s\n", GetAssetName());
            break;
        case (APOLLONNODE_SYNC_SPORKS):
            nTimeLastApollonnodeList = GetTime();
            nRequestedApollonnodeAssets = APOLLONNODE_SYNC_LIST;
            LogPrintf("CApollonnodeSync::SwitchToNextAsset -- Starting %s\n", GetAssetName());
            break;
        case (APOLLONNODE_SYNC_LIST):
            nTimeLastPaymentVote = GetTime();
            nRequestedApollonnodeAssets = APOLLONNODE_SYNC_MNW;
            LogPrintf("CApollonnodeSync::SwitchToNextAsset -- Starting %s\n", GetAssetName());
            break;

        case (APOLLONNODE_SYNC_MNW):
            nTimeLastGovernanceItem = GetTime();
            LogPrintf("CApollonnodeSync::SwitchToNextAsset -- Sync has finished\n");
            nRequestedApollonnodeAssets = APOLLONNODE_SYNC_FINISHED;
            break;
    }
    nRequestedApollonnodeAttempt = 0;
    nTimeAssetSyncStarted = GetTime();
    GetMainSignals().UpdateSyncStatus();
}

std::string CApollonnodeSync::GetSyncStatus() {
    switch (apollonnodeSync.nRequestedApollonnodeAssets) {
        case APOLLONNODE_SYNC_INITIAL:
            return _("Synchronization pending...");
        case APOLLONNODE_SYNC_SPORKS:
            return _("Synchronizing sporks...");
        case APOLLONNODE_SYNC_LIST:
            return _("Synchronizing apollonnodes...");
        case APOLLONNODE_SYNC_MNW:
            return _("Synchronizing apollonnode payments...");
        case APOLLONNODE_SYNC_FAILED:
            return _("Synchronization failed");
        case APOLLONNODE_SYNC_FINISHED:
            return _("Synchronization finished");
        default:
            return "";
    }
}

void CApollonnodeSync::ProcessMessage(CNode *pfrom, std::string &strCommand, CDataStream &vRecv) {
    if (strCommand == NetMsgType::SYNCSTATUSCOUNT) { //Sync status count

        //do not care about stats if sync process finished or failed
        if (IsSynced() || IsFailed()) return;

        int nItemID;
        int nCount;
        vRecv >> nItemID >> nCount;

        LogPrintf("SYNCSTATUSCOUNT -- got inventory count: nItemID=%d  nCount=%d  peer=%d\n", nItemID, nCount, pfrom->id);
    }
}

void CApollonnodeSync::ClearFulfilledRequests() {
    TRY_LOCK(cs_vNodes, lockRecv);
    if (!lockRecv) return;

    BOOST_FOREACH(CNode * pnode, vNodes)
    {
        netfulfilledman.RemoveFulfilledRequest(pnode->addr, "spork-sync");
        netfulfilledman.RemoveFulfilledRequest(pnode->addr, "apollonnode-list-sync");
        netfulfilledman.RemoveFulfilledRequest(pnode->addr, "apollonnode-payment-sync");
        netfulfilledman.RemoveFulfilledRequest(pnode->addr, "full-sync");
    }
}

void CApollonnodeSync::ProcessTick() {
    static int nTick = 0;
    if (nTick++ % APOLLONNODE_SYNC_TICK_SECONDS != 0) return;
    if (!pCurrentBlockApollon) return;

    //the actual count of apollonnodes we have currently
    int nMnCount = mnodeman.CountApollonnodes();

    LogPrint("ProcessTick", "CApollonnodeSync::ProcessTick -- nTick %d nMnCount %d\n", nTick, nMnCount);

    // INITIAL SYNC SETUP / LOG REPORTING
    double nSyncProgress = double(nRequestedApollonnodeAttempt + (nRequestedApollonnodeAssets - 1) * 8) / (8 * 4);
    LogPrint("ProcessTick", "CApollonnodeSync::ProcessTick -- nTick %d nRequestedApollonnodeAssets %d nRequestedApollonnodeAttempt %d nSyncProgress %f\n", nTick, nRequestedApollonnodeAssets, nRequestedApollonnodeAttempt, nSyncProgress);
    uiInterface.NotifyAdditionalDataSyncProgressChanged(pCurrentBlockApollon->nHeight, nSyncProgress);

    // RESET SYNCING INCASE OF FAILURE
    {
        if (IsSynced()) {
            /*
                Resync if we lost all apollonnodes from sleep/wake or failed to sync originally
            */
            if (nMnCount == 0) {
                LogPrintf("CApollonnodeSync::ProcessTick -- WARNING: not enough data, restarting sync\n");
                Reset();
            } else {
                std::vector < CNode * > vNodesCopy = CopyNodeVector();
                ReleaseNodeVector(vNodesCopy);
                return;
            }
        }

        //try syncing again
        if (IsFailed()) {
            if (nTimeLastFailure + (1 * 60) < GetTime()) { // 1 minute cooldown after failed sync
                Reset();
            }
            return;
        }
    }

    if (Params().NetworkIDString() != CBaseChainParams::REGTEST && !IsBlockchainSynced() && nRequestedApollonnodeAssets > APOLLONNODE_SYNC_SPORKS) {
        nTimeLastApollonnodeList = GetTime();
        nTimeLastPaymentVote = GetTime();
        nTimeLastGovernanceItem = GetTime();
        return;
    }
    if (nRequestedApollonnodeAssets == APOLLONNODE_SYNC_INITIAL || (nRequestedApollonnodeAssets == APOLLONNODE_SYNC_SPORKS && IsBlockchainSynced())) {
        SwitchToNextAsset();
    }

    std::vector < CNode * > vNodesCopy = CopyNodeVector();

    BOOST_FOREACH(CNode * pnode, vNodesCopy)
    {
        // Don't try to sync any data from outbound "apollonnode" connections -
        // they are temporary and should be considered unreliable for a sync process.
        // Inbound connection this early is most likely a "apollonnode" connection
        // initialted from another node, so skip it too.
        if (pnode->fApollonnode || (fApollonNode && pnode->fInbound)) continue;

        // QUICK MODE (REGTEST ONLY!)
        if (Params().NetworkIDString() == CBaseChainParams::REGTEST) {
            if (nRequestedApollonnodeAttempt <= 2) {
                pnode->PushMessage(NetMsgType::GETSPORKS); //get current network sporks
            } else if (nRequestedApollonnodeAttempt < 4) {
                mnodeman.DsegUpdate(pnode);
            } else if (nRequestedApollonnodeAttempt < 6) {
                int nMnCount = mnodeman.CountApollonnodes();
                pnode->PushMessage(NetMsgType::APOLLONNODEPAYMENTSYNC, nMnCount); //sync payment votes
            } else {
                nRequestedApollonnodeAssets = APOLLONNODE_SYNC_FINISHED;
                GetMainSignals().UpdateSyncStatus();
            }
            nRequestedApollonnodeAttempt++;
            ReleaseNodeVector(vNodesCopy);
            return;
        }

        // NORMAL NETWORK MODE - TESTNET/MAINNET
        {
            if (netfulfilledman.HasFulfilledRequest(pnode->addr, "full-sync")) {
                // We already fully synced from this node recently,
                // disconnect to free this connection slot for another peer.
                pnode->fDisconnect = true;
                LogPrintf("CApollonnodeSync::ProcessTick -- disconnecting from recently synced peer %d\n", pnode->id);
                continue;
            }

            // SPORK : ALWAYS ASK FOR SPORKS AS WE SYNC (we skip this mode now)

            if (!netfulfilledman.HasFulfilledRequest(pnode->addr, "spork-sync")) {
                // only request once from each peer
                netfulfilledman.AddFulfilledRequest(pnode->addr, "spork-sync");
                // get current network sporks
                pnode->PushMessage(NetMsgType::GETSPORKS);
                LogPrintf("CApollonnodeSync::ProcessTick -- nTick %d nRequestedApollonnodeAssets %d -- requesting sporks from peer %d\n", nTick, nRequestedApollonnodeAssets, pnode->id);
                continue; // always get sporks first, switch to the next node without waiting for the next tick
            }

            // MNLIST : SYNC APOLLONNODE LIST FROM OTHER CONNECTED CLIENTS

            if (nRequestedApollonnodeAssets == APOLLONNODE_SYNC_LIST) {
                // check for timeout first
                if (nTimeLastApollonnodeList < GetTime() - APOLLONNODE_SYNC_TIMEOUT_SECONDS) {
                    LogPrintf("CApollonnodeSync::ProcessTick -- nTick %d nRequestedApollonnodeAssets %d -- timeout\n", nTick, nRequestedApollonnodeAssets);
                    if (nRequestedApollonnodeAttempt == 0) {
                        LogPrintf("CApollonnodeSync::ProcessTick -- ERROR: failed to sync %s\n", GetAssetName());
                        // there is no way we can continue without apollonnode list, fail here and try later
                        Fail();
                        ReleaseNodeVector(vNodesCopy);
                        return;
                    }
                    SwitchToNextAsset();
                    ReleaseNodeVector(vNodesCopy);
                    return;
                }

                // only request once from each peer
                if (netfulfilledman.HasFulfilledRequest(pnode->addr, "apollonnode-list-sync")) continue;
                netfulfilledman.AddFulfilledRequest(pnode->addr, "apollonnode-list-sync");

                if (pnode->nVersion < mnpayments.GetMinApollonnodePaymentsProto()) continue;
                nRequestedApollonnodeAttempt++;

                mnodeman.DsegUpdate(pnode);

                ReleaseNodeVector(vNodesCopy);
                return; //this will cause each peer to get one request each six seconds for the various assets we need
            }

            // MNW : SYNC APOLLONNODE PAYMENT VOTES FROM OTHER CONNECTED CLIENTS

            if (nRequestedApollonnodeAssets == APOLLONNODE_SYNC_MNW) {
                LogPrint("mnpayments", "CApollonnodeSync::ProcessTick -- nTick %d nRequestedApollonnodeAssets %d nTimeLastPaymentVote %lld GetTime() %lld diff %lld\n", nTick, nRequestedApollonnodeAssets, nTimeLastPaymentVote, GetTime(), GetTime() - nTimeLastPaymentVote);
                // check for timeout first
                // This might take a lot longer than APOLLONNODE_SYNC_TIMEOUT_SECONDS minutes due to new blocks,
                // but that should be OK and it should timeout eventually.
                if (nTimeLastPaymentVote < GetTime() - APOLLONNODE_SYNC_TIMEOUT_SECONDS) {
                    LogPrintf("CApollonnodeSync::ProcessTick -- nTick %d nRequestedApollonnodeAssets %d -- timeout\n", nTick, nRequestedApollonnodeAssets);
                    if (nRequestedApollonnodeAttempt == 0) {
                        LogPrintf("CApollonnodeSync::ProcessTick -- ERROR: failed to sync %s\n", GetAssetName());
                        // probably not a good idea to proceed without winner list
                        Fail();
                        ReleaseNodeVector(vNodesCopy);
                        return;
                    }
                    SwitchToNextAsset();
                    ReleaseNodeVector(vNodesCopy);
                    return;
                }

                // check for data
                // if mnpayments already has enough blocks and votes, switch to the next asset
                // try to fetch data from at least two peers though
                if (nRequestedApollonnodeAttempt > 1 && mnpayments.IsEnoughData()) {
                    LogPrintf("CApollonnodeSync::ProcessTick -- nTick %d nRequestedApollonnodeAssets %d -- found enough data\n", nTick, nRequestedApollonnodeAssets);
                    SwitchToNextAsset();
                    ReleaseNodeVector(vNodesCopy);
                    return;
                }

                // only request once from each peer
                if (netfulfilledman.HasFulfilledRequest(pnode->addr, "apollonnode-payment-sync")) continue;
                netfulfilledman.AddFulfilledRequest(pnode->addr, "apollonnode-payment-sync");

                if (pnode->nVersion < mnpayments.GetMinApollonnodePaymentsProto()) continue;
                nRequestedApollonnodeAttempt++;

                // ask node for all payment votes it has (new nodes will only return votes for future payments)
                pnode->PushMessage(NetMsgType::APOLLONNODEPAYMENTSYNC, mnpayments.GetStorageLimit());
                // ask node for missing pieces only (old nodes will not be asked)
                mnpayments.RequestLowDataPaymentBlocks(pnode);

                ReleaseNodeVector(vNodesCopy);
                return; //this will cause each peer to get one request each six seconds for the various assets we need
            }

        }
    }
    // looped through all nodes, release them
    ReleaseNodeVector(vNodesCopy);
}

void CApollonnodeSync::UpdatedBlockTip(const CBlockApollon *papollon) {
    pCurrentBlockApollon = papollon;
}
