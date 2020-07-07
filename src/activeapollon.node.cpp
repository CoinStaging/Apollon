// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activeapollonnode.h"
#include "consensus/consensus.h"
#include "apollonnode.h"
#include "apollonnode-sync.h"
#include "apollonnode-payments.h"
#include "apollonnodeman.h"
#include "protocol.h"
#include "validationinterface.h"

extern CWallet *pwalletMain;

// Keep track of the active Apollonnode
CActiveApollonnode activeApollonnode;

void CActiveApollonnode::ManageState() {
    LogPrint("apollonnode", "CActiveApollonnode::ManageState -- Start\n");
    if (!fApollonNode) {
        LogPrint("apollonnode", "CActiveApollonnode::ManageState -- Not a apollonnode, returning\n");
        return;
    }

    if (Params().NetworkIDString() != CBaseChainParams::REGTEST && !apollonnodeSync.GetBlockchainSynced()) {
        ChangeState(ACTIVE_APOLLONNODE_SYNC_IN_PROCESS);
        LogPrintf("CActiveApollonnode::ManageState -- %s: %s\n", GetStateString(), GetStatus());
        return;
    }

    if (nState == ACTIVE_APOLLONNODE_SYNC_IN_PROCESS) {
        ChangeState(ACTIVE_APOLLONNODE_INITIAL);
    }

    LogPrint("apollonnode", "CActiveApollonnode::ManageState -- status = %s, type = %s, pinger enabled = %d\n",
             GetStatus(), GetTypeString(), fPingerEnabled);

    if (eType == APOLLONNODE_UNKNOWN) {
        ManageStateInitial();
    }

    if (eType == APOLLONNODE_REMOTE) {
        ManageStateRemote();
    } else if (eType == APOLLONNODE_LOCAL) {
        // Try Remote Start first so the started local apollonnode can be restarted without recreate apollonnode broadcast.
        ManageStateRemote();
        if (nState != ACTIVE_APOLLONNODE_STARTED)
            ManageStateLocal();
    }

    SendApollonnodePing();
}

std::string CActiveApollonnode::GetStateString() const {
    switch (nState) {
        case ACTIVE_APOLLONNODE_INITIAL:
            return "INITIAL";
        case ACTIVE_APOLLONNODE_SYNC_IN_PROCESS:
            return "SYNC_IN_PROCESS";
        case ACTIVE_APOLLONNODE_INPUT_TOO_NEW:
            return "INPUT_TOO_NEW";
        case ACTIVE_APOLLONNODE_NOT_CAPABLE:
            return "NOT_CAPABLE";
        case ACTIVE_APOLLONNODE_STARTED:
            return "STARTED";
        default:
            return "UNKNOWN";
    }
}

void CActiveApollonnode::ChangeState(int state) {
    if(nState!=state){
        nState = state;
    }
}

std::string CActiveApollonnode::GetStatus() const {
    switch (nState) {
        case ACTIVE_APOLLONNODE_INITIAL:
            return "Node just started, not yet activated";
        case ACTIVE_APOLLONNODE_SYNC_IN_PROCESS:
            return "Sync in progress. Must wait until sync is complete to start Apollonnode";
        case ACTIVE_APOLLONNODE_INPUT_TOO_NEW:
            return strprintf("Apollonnode input must have at least %d confirmations",
                             Params().GetConsensus().nApollonnodeMinimumConfirmations);
        case ACTIVE_APOLLONNODE_NOT_CAPABLE:
            return "Not capable apollonnode: " + strNotCapableReason;
        case ACTIVE_APOLLONNODE_STARTED:
            return "Apollonnode successfully started";
        default:
            return "Unknown";
    }
}

std::string CActiveApollonnode::GetTypeString() const {
    std::string strType;
    switch (eType) {
        case APOLLONNODE_UNKNOWN:
            strType = "UNKNOWN";
            break;
        case APOLLONNODE_REMOTE:
            strType = "REMOTE";
            break;
        case APOLLONNODE_LOCAL:
            strType = "LOCAL";
            break;
        default:
            strType = "UNKNOWN";
            break;
    }
    return strType;
}

bool CActiveApollonnode::SendApollonnodePing() {
    if (!fPingerEnabled) {
        LogPrint("apollonnode",
                 "CActiveApollonnode::SendApollonnodePing -- %s: apollonnode ping service is disabled, skipping...\n",
                 GetStateString());
        return false;
    }

    if (!mnodeman.Has(vin)) {
        strNotCapableReason = "Apollonnode not in apollonnode list";
        ChangeState(ACTIVE_APOLLONNODE_NOT_CAPABLE);
        LogPrintf("CActiveApollonnode::SendApollonnodePing -- %s: %s\n", GetStateString(), strNotCapableReason);
        return false;
    }

    CApollonnodePing mnp(vin);
    if (!mnp.Sign(keyApollonnode, pubKeyApollonnode)) {
        LogPrintf("CActiveApollonnode::SendApollonnodePing -- ERROR: Couldn't sign Apollonnode Ping\n");
        return false;
    }

    // Update lastPing for our apollonnode in Apollonnode list
    if (mnodeman.IsApollonnodePingedWithin(vin, APOLLONNODE_MIN_MNP_SECONDS, mnp.sigTime)) {
        LogPrintf("CActiveApollonnode::SendApollonnodePing -- Too early to send Apollonnode Ping\n");
        return false;
    }

    mnodeman.SetApollonnodeLastPing(vin, mnp);

    LogPrintf("CActiveApollonnode::SendApollonnodePing -- Relaying ping, collateral=%s\n", vin.ToString());
    mnp.Relay();

    return true;
}

void CActiveApollonnode::ManageStateInitial() {
    LogPrint("apollonnode", "CActiveApollonnode::ManageStateInitial -- status = %s, type = %s, pinger enabled = %d\n",
             GetStatus(), GetTypeString(), fPingerEnabled);

    // Check that our local network configuration is correct
    if (!fListen) {
        // listen option is probably overwritten by smth else, no good
        ChangeState(ACTIVE_APOLLONNODE_NOT_CAPABLE);
        strNotCapableReason = "Apollonnode must accept connections from outside. Make sure listen configuration option is not overwritten by some another parameter.";
        LogPrintf("CActiveApollonnode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    bool fFoundLocal = false;
    {
        LOCK(cs_vNodes);

        // First try to find whatever local address is specified by externalip option
        fFoundLocal = GetLocal(service) && CApollonnode::IsValidNetAddr(service);
        if (!fFoundLocal) {
            // nothing and no live connections, can't do anything for now
            if (vNodes.empty()) {
                ChangeState(ACTIVE_APOLLONNODE_NOT_CAPABLE);
                strNotCapableReason = "Can't detect valid external address. Will retry when there are some connections available.";
                LogPrintf("CActiveApollonnode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
                return;
            }
            // We have some peers, let's try to find our local address from one of them
            BOOST_FOREACH(CNode * pnode, vNodes)
            {
                if (pnode->fSuccessfullyConnected && pnode->addr.IsIPv4()) {
                    fFoundLocal = GetLocal(service, &pnode->addr) && CApollonnode::IsValidNetAddr(service);
                    if (fFoundLocal) break;
                }
            }
        }
    }

    if (!fFoundLocal) {
        ChangeState(ACTIVE_APOLLONNODE_NOT_CAPABLE);
        strNotCapableReason = "Can't detect valid external address. Please consider using the externalip configuration option if problem persists. Make sure to use IPv4 address only.";
        LogPrintf("CActiveApollonnode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    int mainnetDefaultPort = Params(CBaseChainParams::MAIN).GetDefaultPort();
    if (Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if (service.GetPort() != mainnetDefaultPort) {
            ChangeState(ACTIVE_APOLLONNODE_NOT_CAPABLE);
            strNotCapableReason = strprintf("Invalid port: %u - only %d is supported on mainnet.", service.GetPort(),
                                            mainnetDefaultPort);
            LogPrintf("CActiveApollonnode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
    } else if (service.GetPort() == mainnetDefaultPort) {
        ChangeState(ACTIVE_APOLLONNODE_NOT_CAPABLE);
        strNotCapableReason = strprintf("Invalid port: %u - %d is only supported on mainnet.", service.GetPort(),
                                        mainnetDefaultPort);
        LogPrintf("CActiveApollonnode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    LogPrintf("CActiveApollonnode::ManageStateInitial -- Checking inbound connection to '%s'\n", service.ToString());
    //TODO
    if (!ConnectNode(CAddress(service, NODE_NETWORK), NULL, false, true)) {
        ChangeState(ACTIVE_APOLLONNODE_NOT_CAPABLE);
        strNotCapableReason = "Could not connect to " + service.ToString();
        LogPrintf("CActiveApollonnode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    // Default to REMOTE
    eType = APOLLONNODE_REMOTE;

    // Check if wallet funds are available
    if (!pwalletMain) {
        LogPrintf("CActiveApollonnode::ManageStateInitial -- %s: Wallet not available\n", GetStateString());
        return;
    }

    if (pwalletMain->IsLocked()) {
        LogPrintf("CActiveApollonnode::ManageStateInitial -- %s: Wallet is locked\n", GetStateString());
        return;
    }

    if (pwalletMain->GetBalance() < APOLLONNODE_COIN_REQUIRED * COIN) {
        LogPrintf("CActiveApollonnode::ManageStateInitial -- %s: Wallet balance is < 1000 XAP\n", GetStateString());
        return;
    }

    // Choose coins to use
    CPubKey pubKeyCollateral;
    CKey keyCollateral;

    // If collateral is found switch to LOCAL mode
    if (pwalletMain->GetApollonnodeVinAndKeys(vin, pubKeyCollateral, keyCollateral)) {
        eType = APOLLONNODE_LOCAL;
    }

    LogPrint("apollonnode", "CActiveApollonnode::ManageStateInitial -- End status = %s, type = %s, pinger enabled = %d\n",
             GetStatus(), GetTypeString(), fPingerEnabled);
}

void CActiveApollonnode::ManageStateRemote() {
    LogPrint("apollonnode",
             "CActiveApollonnode::ManageStateRemote -- Start status = %s, type = %s, pinger enabled = %d, pubKeyApollonnode.GetID() = %s\n",
             GetStatus(), fPingerEnabled, GetTypeString(), pubKeyApollonnode.GetID().ToString());

    mnodeman.CheckApollonnode(pubKeyApollonnode);
    apollonnode_info_t infoMn = mnodeman.GetApollonnodeInfo(pubKeyApollonnode);

    if (infoMn.fInfoValid) {
        if (infoMn.nProtocolVersion < MIN_APOLLONNODE_PAYMENT_PROTO_VERSION_1 || infoMn.nProtocolVersion > MIN_APOLLONNODE_PAYMENT_PROTO_VERSION_2) {
            ChangeState(ACTIVE_APOLLONNODE_NOT_CAPABLE);
            strNotCapableReason = "Invalid protocol version";
            LogPrintf("CActiveApollonnode::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
        if (service != infoMn.addr) {
            ChangeState(ACTIVE_APOLLONNODE_NOT_CAPABLE);
            // LogPrintf("service: %s\n", service.ToString());
            // LogPrintf("infoMn.addr: %s\n", infoMn.addr.ToString());
            strNotCapableReason = "Broadcasted IP doesn't match our external address. Make sure you issued a new broadcast if IP of this apollonnode changed recently.";
            LogPrintf("CActiveApollonnode::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
        if (!CApollonnode::IsValidStateForAutoStart(infoMn.nActiveState)) {
            ChangeState(ACTIVE_APOLLONNODE_NOT_CAPABLE);
            strNotCapableReason = strprintf("Apollonnode in %s state", CApollonnode::StateToString(infoMn.nActiveState));
            LogPrintf("CActiveApollonnode::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
        if (nState != ACTIVE_APOLLONNODE_STARTED) {
            LogPrintf("CActiveApollonnode::ManageStateRemote -- STARTED!\n");
            vin = infoMn.vin;
            service = infoMn.addr;
            fPingerEnabled = true;
            ChangeState(ACTIVE_APOLLONNODE_STARTED);
        }
    } else {
        ChangeState(ACTIVE_APOLLONNODE_NOT_CAPABLE);
        strNotCapableReason = "Apollonnode not in apollonnode list";
        LogPrintf("CActiveApollonnode::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
    }
}

void CActiveApollonnode::ManageStateLocal() {
    LogPrint("apollonnode", "CActiveApollonnode::ManageStateLocal -- status = %s, type = %s, pinger enabled = %d\n",
             GetStatus(), GetTypeString(), fPingerEnabled);
    if (nState == ACTIVE_APOLLONNODE_STARTED) {
        return;
    }

    // Choose coins to use
    CPubKey pubKeyCollateral;
    CKey keyCollateral;

    if (pwalletMain->GetApollonnodeVinAndKeys(vin, pubKeyCollateral, keyCollateral)) {
        int nInputAge = GetInputAge(vin);
        if (nInputAge < Params().GetConsensus().nApollonnodeMinimumConfirmations) {
            ChangeState(ACTIVE_APOLLONNODE_INPUT_TOO_NEW);
            strNotCapableReason = strprintf(_("%s - %d confirmations"), GetStatus(), nInputAge);
            LogPrintf("CActiveApollonnode::ManageStateLocal -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }

        {
            LOCK(pwalletMain->cs_wallet);
            pwalletMain->LockCoin(vin.prevout);
        }

        CApollonnodeBroadcast mnb;
        std::string strError;
        if (!CApollonnodeBroadcast::Create(vin, service, keyCollateral, pubKeyCollateral, keyApollonnode,
                                     pubKeyApollonnode, strError, mnb)) {
            ChangeState(ACTIVE_APOLLONNODE_NOT_CAPABLE);
            strNotCapableReason = "Error creating mastenode broadcast: " + strError;
            LogPrintf("CActiveApollonnode::ManageStateLocal -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }

        fPingerEnabled = true;
        ChangeState(ACTIVE_APOLLONNODE_STARTED);

        //update to apollonnode list
        LogPrintf("CActiveApollonnode::ManageStateLocal -- Update Apollonnode List\n");
        mnodeman.UpdateApollonnodeList(mnb);
        mnodeman.NotifyApollonnodeUpdates();

        //send to all peers
        LogPrintf("CActiveApollonnode::ManageStateLocal -- Relay broadcast, vin=%s\n", vin.ToString());
        mnb.RelayApollonNode();
    }
}
