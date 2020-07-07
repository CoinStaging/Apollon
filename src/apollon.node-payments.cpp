// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activeapollonnode.h"
#include "darksend.h"
#include "apollonnode-payments.h"
#include "apollonnode-sync.h"
#include "apollonnodeman.h"
#include "netfulfilledman.h"
#include "spork.h"
#include "util.h"

#include <boost/lexical_cast.hpp>

/** Object for who's going to get paid on which blocks */
CApollonnodePayments mnpayments;

CCriticalSection cs_vecPayees;
CCriticalSection cs_mapApollonnodeBlocks;
CCriticalSection cs_mapApollonnodePaymentVotes;

/**
* IsBlockValueValid
*
*   Determine if coinbase outgoing created money is the correct value
*
*   Why is this needed?
*   - In Apollon some blocks are superblocks, which output much higher amounts of coins
*   - Otherblocks are 10% lower in outgoing value, so in total, no extra coins are created
*   - When non-superblocks are detected, the normal schedule should be maintained
*/

bool IsBlockValueValid(const CBlock &block, int nBlockHeight, CAmount blockReward, std::string &strErrorRet) {
    strErrorRet = "";

    bool isBlockRewardValueMet = (block.vtx[0].GetValueOut() <= blockReward);
    if (fDebug) LogPrintf("block.vtx[0].GetValueOut() %lld <= blockReward %lld\n", block.vtx[0].GetValueOut(), blockReward);

    // we are still using budgets, but we have no data about them anymore,
    // all we know is predefined budget cycle and window

//    const Consensus::Params &consensusParams = Params().GetConsensus();
//
////    if (nBlockHeight < consensusParams.nSuperblockStartBlock) {
//        int nOffset = nBlockHeight % consensusParams.nBudgetPaymentsCycleBlocks;
//        if (nBlockHeight >= consensusParams.nBudgetPaymentsStartBlock &&
//            nOffset < consensusParams.nBudgetPaymentsWindowBlocks) {
//            // NOTE: make sure SPORK_13_OLD_SUPERBLOCK_FLAG is disabled when 12.1 starts to go live
//            if (apollonnodeSync.IsSynced() && !sporkManager.IsSporkActive(SPORK_13_OLD_SUPERBLOCK_FLAG)) {
//                // no budget blocks should be accepted here, if SPORK_13_OLD_SUPERBLOCK_FLAG is disabled
//                LogPrint("gobject", "IsBlockValueValid -- Client synced but budget spork is disabled, checking block value against block reward\n");
//                if (!isBlockRewardValueMet) {
//                    strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, budgets are disabled",
//                                            nBlockHeight, block.vtx[0].GetValueOut(), blockReward);
//                }
//                return isBlockRewardValueMet;
//            }
//            LogPrint("gobject", "IsBlockValueValid -- WARNING: Skipping budget block value checks, accepting block\n");
//            // TODO: reprocess blocks to make sure they are legit?
//            return true;
//        }
//        // LogPrint("gobject", "IsBlockValueValid -- Block is not in budget cycle window, checking block value against block reward\n");
//        if (!isBlockRewardValueMet) {
//            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, block is not in budget cycle window",
//                                    nBlockHeight, block.vtx[0].GetValueOut(), blockReward);
//        }
//        return isBlockRewardValueMet;
//    }

    // superblocks started

//    CAmount nSuperblockMaxValue =  blockReward + CSuperblock::GetPaymentsLimit(nBlockHeight);
//    bool isSuperblockMaxValueMet = (block.vtx[0].GetValueOut() <= nSuperblockMaxValue);
//    bool isSuperblockMaxValueMet = false;

//    LogPrint("gobject", "block.vtx[0].GetValueOut() %lld <= nSuperblockMaxValue %lld\n", block.vtx[0].GetValueOut(), nSuperblockMaxValue);

    if (!apollonnodeSync.IsSynced()) {
        // not enough data but at least it must NOT exceed superblock max value
//        if(CSuperblock::IsValidBlockHeight(nBlockHeight)) {
//            if(fDebug) LogPrintf("IsBlockPayeeValid -- WARNING: Client not synced, checking superblock max bounds only\n");
//            if(!isSuperblockMaxValueMet) {
//                strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded superblock max value",
//                                        nBlockHeight, block.vtx[0].GetValueOut(), nSuperblockMaxValue);
//            }
//            return isSuperblockMaxValueMet;
//        }
        if (!isBlockRewardValueMet) {
            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, only regular blocks are allowed at this height",
                                    nBlockHeight, block.vtx[0].GetValueOut(), blockReward);
        }
        // it MUST be a regular block otherwise
        return isBlockRewardValueMet;
    }

    // we are synced, let's try to check as much data as we can

    if (sporkManager.IsSporkActive(SPORK_9_SUPERBLOCKS_ENABLED)) {
////        if(CSuperblockManager::IsSuperblockTriggered(nBlockHeight)) {
////            if(CSuperblockManager::IsValid(block.vtx[0], nBlockHeight, blockReward)) {
////                LogPrint("gobject", "IsBlockValueValid -- Valid superblock at height %d: %s", nBlockHeight, block.vtx[0].ToString());
////                // all checks are done in CSuperblock::IsValid, nothing to do here
////                return true;
////            }
////
////            // triggered but invalid? that's weird
////            LogPrintf("IsBlockValueValid -- ERROR: Invalid superblock detected at height %d: %s", nBlockHeight, block.vtx[0].ToString());
////            // should NOT allow invalid superblocks, when superblocks are enabled
////            strErrorRet = strprintf("invalid superblock detected at height %d", nBlockHeight);
////            return false;
////        }
//        LogPrint("gobject", "IsBlockValueValid -- No triggered superblock detected at height %d\n", nBlockHeight);
//        if(!isBlockRewardValueMet) {
//            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, no triggered superblock detected",
//                                    nBlockHeight, block.vtx[0].GetValueOut(), blockReward);
//        }
    } else {
//        // should NOT allow superblocks at all, when superblocks are disabled
        LogPrint("gobject", "IsBlockValueValid -- Superblocks are disabled, no superblocks allowed\n");
        if (!isBlockRewardValueMet) {
            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, superblocks are disabled",
                                    nBlockHeight, block.vtx[0].GetValueOut(), blockReward);
        }
    }

    // it MUST be a regular block
    return isBlockRewardValueMet;
}

bool IsBlockPayeeValid(const CTransaction &txNew, int nBlockHeight, CAmount blockReward) {
    // we can only check apollonnode payment /
    const Consensus::Params &consensusParams = Params().GetConsensus();

    if (nBlockHeight < consensusParams.nApollonnodePaymentsStartBlock) {
        //there is no budget data to use to check anything, let's just accept the longest chain
        if (fDebug) LogPrintf("IsBlockPayeeValid -- apollonnode isn't start\n");
        return true;
    }
    if (!apollonnodeSync.IsSynced() && Params().NetworkIDString() != CBaseChainParams::REGTEST) {
        //there is no budget data to use to check anything, let's just accept the longest chain
        if (fDebug) LogPrintf("IsBlockPayeeValid -- WARNING: Client not synced, skipping block payee checks\n");
        return true;
    }

    //check for apollonnode payee
    if (mnpayments.IsTransactionValid(txNew, nBlockHeight, false)) {
        LogPrint("mnpayments", "IsBlockPayeeValid -- Valid apollonnode payment at height %d: %s", nBlockHeight, txNew.ToString());
        return true;
    } else {
        if(sporkManager.IsSporkActive(SPORK_8_APOLLONNODE_PAYMENT_ENFORCEMENT)){
            return false;
        } else {
            LogPrintf("ApollonNode payment enforcement is disabled, accepting block\n");
            return true;
        }
    }
}

void FillBlockPayments(CMutableTransaction &txNew, int nBlockHeight, CAmount apollonnodePayment, CTxOut &txoutApollonnodeRet, std::vector <CTxOut> &voutSuperblockRet) {
    // only create superblocks if spork is enabled AND if superblock is actually triggered
    // (height should be validated inside)
//    if(sporkManager.IsSporkActive(SPORK_9_SUPERBLOCKS_ENABLED) &&
//        CSuperblockManager::IsSuperblockTriggered(nBlockHeight)) {
//            LogPrint("gobject", "FillBlockPayments -- triggered superblock creation at height %d\n", nBlockHeight);
//            CSuperblockManager::CreateSuperblock(txNew, nBlockHeight, voutSuperblockRet);
//            return;
//    }

    // FILL BLOCK PAYEE WITH APOLLONNODE PAYMENT OTHERWISE
    mnpayments.FillBlockPayee(txNew, nBlockHeight, apollonnodePayment, txoutApollonnodeRet);
    LogPrint("mnpayments", "FillBlockPayments -- nBlockHeight %d apollonnodePayment %lld txoutApollonnodeRet %s txNew %s",
             nBlockHeight, apollonnodePayment, txoutApollonnodeRet.ToString(), txNew.ToString());
}

std::string GetRequiredPaymentsString(int nBlockHeight) {
    // IF WE HAVE A ACTIVATED TRIGGER FOR THIS HEIGHT - IT IS A SUPERBLOCK, GET THE REQUIRED PAYEES
//    if(CSuperblockManager::IsSuperblockTriggered(nBlockHeight)) {
//        return CSuperblockManager::GetRequiredPaymentsString(nBlockHeight);
//    }

    // OTHERWISE, PAY APOLLONNODE
    return mnpayments.GetRequiredPaymentsString(nBlockHeight);
}

void CApollonnodePayments::Clear() {
    LOCK2(cs_mapApollonnodeBlocks, cs_mapApollonnodePaymentVotes);
    mapApollonnodeBlocks.clear();
    mapApollonnodePaymentVotes.clear();
}

bool CApollonnodePayments::CanVote(COutPoint outApollonnode, int nBlockHeight) {
    LOCK(cs_mapApollonnodePaymentVotes);

    if (mapApollonnodesLastVote.count(outApollonnode) && mapApollonnodesLastVote[outApollonnode] == nBlockHeight) {
        return false;
    }

    //record this apollonnode voted
    mapApollonnodesLastVote[outApollonnode] = nBlockHeight;
    return true;
}

std::string CApollonnodePayee::ToString() const {
    CTxDestination address1;
    ExtractDestination(scriptPubKey, address1);
    CBitcoinAddress address2(address1);
    std::string str;
    str += "(address: ";
    str += address2.ToString();
    str += ")\n";
    return str;
}

/**
*   FillBlockPayee
*
*   Fill Apollonnode ONLY payment block
*/

void CApollonnodePayments::FillBlockPayee(CMutableTransaction &txNew, int nBlockHeight, CAmount apollonnodePayment, CTxOut &txoutApollonnodeRet) {
    // make sure it's not filled yet
    txoutApollonnodeRet = CTxOut();

    CScript payee;
    bool foundMaxVotedPayee = true;

    if (!mnpayments.GetBlockPayee(nBlockHeight, payee)) {
        // no apollonnode detected...
        // LogPrintf("no apollonnode detected...\n");
        foundMaxVotedPayee = false;
        int nCount = 0;
        CApollonnode *winningNode = mnodeman.GetNextApollonnodeInQueueForPayment(nBlockHeight, true, nCount);
        if (!winningNode) {
            if(Params().NetworkIDString() != CBaseChainParams::REGTEST) {
                // ...and we can't calculate it on our own
                LogPrintf("CApollonnodePayments::FillBlockPayee -- Failed to detect apollonnode to pay\n");
                return;
            }
        }
        // fill payee with locally calculated winner and hope for the best
        if (winningNode) {
            payee = GetScriptForDestination(winningNode->pubKeyCollateralAddress.GetID());
            LogPrintf("payee=%s\n", winningNode->ToString());
        }
        else
            payee = txNew.vout[0].scriptPubKey;//This is only for unit tests scenario on REGTEST
    }
    txoutApollonnodeRet = CTxOut(apollonnodePayment, payee);
    txNew.vout.push_back(txoutApollonnodeRet);

    CTxDestination address1;
    ExtractDestination(payee, address1);
    CBitcoinAddress address2(address1);
    if (foundMaxVotedPayee) {
        LogPrintf("CApollonnodePayments::FillBlockPayee::foundMaxVotedPayee -- Apollonnode payment %lld to %s\n", apollonnodePayment, address2.ToString());
    } else {
        LogPrintf("CApollonnodePayments::FillBlockPayee -- Apollonnode payment %lld to %s\n", apollonnodePayment, address2.ToString());
    }

}

int CApollonnodePayments::GetMinApollonnodePaymentsProto() {
    return sporkManager.IsSporkActive(SPORK_10_APOLLONNODE_PAY_UPDATED_NODES)
           ? MIN_APOLLONNODE_PAYMENT_PROTO_VERSION_2
           : MIN_APOLLONNODE_PAYMENT_PROTO_VERSION_1;
}

void CApollonnodePayments::ProcessMessage(CNode *pfrom, std::string &strCommand, CDataStream &vRecv) {

//    LogPrintf("CApollonnodePayments::ProcessMessage strCommand=%s\n", strCommand);
    // Ignore any payments messages until apollonnode list is synced
    if (!apollonnodeSync.IsApollonnodeListSynced()) return;

    if (fLiteMode) return; // disable all Apollon specific functionality

    bool fTestNet = (Params().NetworkIDString() == CBaseChainParams::TESTNET);

    if (strCommand == NetMsgType::APOLLONNODEPAYMENTSYNC) { //Apollonnode Payments Request Sync

        // Ignore such requests until we are fully synced.
        // We could start processing this after apollonnode list is synced
        // but this is a heavy one so it's better to finish sync first.
        if (!apollonnodeSync.IsSynced()) return;

        int nCountNeeded;
        vRecv >> nCountNeeded;

        if (netfulfilledman.HasFulfilledRequest(pfrom->addr, NetMsgType::APOLLONNODEPAYMENTSYNC)) {
            // Asking for the payments list multiple times in a short period of time is no good
            LogPrintf("APOLLONNODEPAYMENTSYNC -- peer already asked me for the list, peer=%d\n", pfrom->id);
            if (!fTestNet) Misbehaving(pfrom->GetId(), 20);
            return;
        }
        netfulfilledman.AddFulfilledRequest(pfrom->addr, NetMsgType::APOLLONNODEPAYMENTSYNC);

        Sync(pfrom);
        LogPrint("mnpayments", "APOLLONNODEPAYMENTSYNC -- Sent Apollonnode payment votes to peer %d\n", pfrom->id);

    } else if (strCommand == NetMsgType::APOLLONNODEPAYMENTVOTE) { // Apollonnode Payments Vote for the Winner

        CApollonnodePaymentVote vote;
        vRecv >> vote;

        if (pfrom->nVersion < GetMinApollonnodePaymentsProto()) return;

        if (!pCurrentBlockApollon) return;

        uint256 nHash = vote.GetHash();

        pfrom->setAskFor.erase(nHash);

        {
            LOCK(cs_mapApollonnodePaymentVotes);
            if (mapApollonnodePaymentVotes.count(nHash)) {
                LogPrint("mnpayments", "APOLLONNODEPAYMENTVOTE -- hash=%s, nHeight=%d seen\n", nHash.ToString(), pCurrentBlockApollon->nHeight);
                return;
            }

            // Avoid processing same vote multiple times
            mapApollonnodePaymentVotes[nHash] = vote;
            // but first mark vote as non-verified,
            // AddPaymentVote() below should take care of it if vote is actually ok
            mapApollonnodePaymentVotes[nHash].MarkAsNotVerified();
        }

        int nFirstBlock = pCurrentBlockApollon->nHeight - GetStorageLimit();
        if (vote.nBlockHeight < nFirstBlock || vote.nBlockHeight > pCurrentBlockApollon->nHeight + 20) {
            LogPrint("mnpayments", "APOLLONNODEPAYMENTVOTE -- vote out of range: nFirstBlock=%d, nBlockHeight=%d, nHeight=%d\n", nFirstBlock, vote.nBlockHeight, pCurrentBlockApollon->nHeight);
            return;
        }

        std::string strError = "";
        if (!vote.IsValid(pfrom, pCurrentBlockApollon->nHeight, strError)) {
            LogPrint("mnpayments", "APOLLONNODEPAYMENTVOTE -- invalid message, error: %s\n", strError);
            return;
        }

        if (!CanVote(vote.vinApollonnode.prevout, vote.nBlockHeight)) {
            LogPrintf("APOLLONNODEPAYMENTVOTE -- apollonnode already voted, apollonnode=%s\n", vote.vinApollonnode.prevout.ToStringShort());
            return;
        }

        apollonnode_info_t mnInfo = mnodeman.GetApollonnodeInfo(vote.vinApollonnode);
        if (!mnInfo.fInfoValid) {
            // mn was not found, so we can't check vote, some info is probably missing
            LogPrintf("APOLLONNODEPAYMENTVOTE -- apollonnode is missing %s\n", vote.vinApollonnode.prevout.ToStringShort());
            mnodeman.AskForMN(pfrom, vote.vinApollonnode);
            return;
        }

        int nDos = 0;
        if (!vote.CheckSignature(mnInfo.pubKeyApollonnode, pCurrentBlockApollon->nHeight, nDos)) {
            if (nDos) {
                LogPrintf("APOLLONNODEPAYMENTVOTE -- ERROR: invalid signature\n");
                if (!fTestNet) Misbehaving(pfrom->GetId(), nDos);
            } else {
                // only warn about anything non-critical (i.e. nDos == 0) in debug mode
                LogPrint("mnpayments", "APOLLONNODEPAYMENTVOTE -- WARNING: invalid signature\n");
            }
            // Either our info or vote info could be outdated.
            // In case our info is outdated, ask for an update,
            mnodeman.AskForMN(pfrom, vote.vinApollonnode);
            // but there is nothing we can do if vote info itself is outdated
            // (i.e. it was signed by a mn which changed its key),
            // so just quit here.
            return;
        }

        CTxDestination address1;
        ExtractDestination(vote.payee, address1);
        CBitcoinAddress address2(address1);

        LogPrint("mnpayments", "APOLLONNODEPAYMENTVOTE -- vote: address=%s, nBlockHeight=%d, nHeight=%d, prevout=%s\n", address2.ToString(), vote.nBlockHeight, pCurrentBlockApollon->nHeight, vote.vinApollonnode.prevout.ToStringShort());

        if (AddPaymentVote(vote)) {
            vote.Relay();
            apollonnodeSync.AddedPaymentVote();
        }
    }
}

bool CApollonnodePaymentVote::Sign() {
    std::string strError;
    std::string strMessage = vinApollonnode.prevout.ToStringShort() +
                             boost::lexical_cast<std::string>(nBlockHeight) +
                             ScriptToAsmStr(payee);

    if (!darkSendSigner.SignMessage(strMessage, vchSig, activeApollonnode.keyApollonnode)) {
        LogPrintf("CApollonnodePaymentVote::Sign -- SignMessage() failed\n");
        return false;
    }

    if (!darkSendSigner.VerifyMessage(activeApollonnode.pubKeyApollonnode, vchSig, strMessage, strError)) {
        LogPrintf("CApollonnodePaymentVote::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CApollonnodePayments::GetBlockPayee(int nBlockHeight, CScript &payee) {
    if (mapApollonnodeBlocks.count(nBlockHeight)) {
        return mapApollonnodeBlocks[nBlockHeight].GetBestPayee(payee);
    }

    return false;
}

// Is this apollonnode scheduled to get paid soon?
// -- Only look ahead up to 8 blocks to allow for propagation of the latest 2 blocks of votes
bool CApollonnodePayments::IsScheduled(CApollonnode &mn, int nNotBlockHeight) {
    LOCK(cs_mapApollonnodeBlocks);

    if (!pCurrentBlockApollon) return false;

    CScript mnpayee;
    mnpayee = GetScriptForDestination(mn.pubKeyCollateralAddress.GetID());

    CScript payee;
    for (int64_t h = pCurrentBlockApollon->nHeight; h <= pCurrentBlockApollon->nHeight + 8; h++) {
        if (h == nNotBlockHeight) continue;
        if (mapApollonnodeBlocks.count(h) && mapApollonnodeBlocks[h].GetBestPayee(payee) && mnpayee == payee) {
            return true;
        }
    }

    return false;
}

bool CApollonnodePayments::AddPaymentVote(const CApollonnodePaymentVote &vote) {
    LogPrint("apollonnode-payments", "CApollonnodePayments::AddPaymentVote\n");
    uint256 blockHash = uint256();
    if (!GetBlockHash(blockHash, vote.nBlockHeight - 101)) return false;

    if (HasVerifiedPaymentVote(vote.GetHash())) return false;

    LOCK2(cs_mapApollonnodeBlocks, cs_mapApollonnodePaymentVotes);

    mapApollonnodePaymentVotes[vote.GetHash()] = vote;

    if (!mapApollonnodeBlocks.count(vote.nBlockHeight)) {
        CApollonnodeBlockPayees blockPayees(vote.nBlockHeight);
        mapApollonnodeBlocks[vote.nBlockHeight] = blockPayees;
    }

    mapApollonnodeBlocks[vote.nBlockHeight].AddPayee(vote);

    return true;
}

bool CApollonnodePayments::HasVerifiedPaymentVote(uint256 hashIn) {
    LOCK(cs_mapApollonnodePaymentVotes);
    std::map<uint256, CApollonnodePaymentVote>::iterator it = mapApollonnodePaymentVotes.find(hashIn);
    return it != mapApollonnodePaymentVotes.end() && it->second.IsVerified();
}

void CApollonnodeBlockPayees::AddPayee(const CApollonnodePaymentVote &vote) {
    LOCK(cs_vecPayees);

    BOOST_FOREACH(CApollonnodePayee & payee, vecPayees)
    {
        if (payee.GetPayee() == vote.payee) {
            payee.AddVoteHash(vote.GetHash());
            return;
        }
    }
    CApollonnodePayee payeeNew(vote.payee, vote.GetHash());
    vecPayees.push_back(payeeNew);
}

bool CApollonnodeBlockPayees::GetBestPayee(CScript &payeeRet) {
    LOCK(cs_vecPayees);
    LogPrint("mnpayments", "CApollonnodeBlockPayees::GetBestPayee, vecPayees.size()=%s\n", vecPayees.size());
    if (!vecPayees.size()) {
        LogPrint("mnpayments", "CApollonnodeBlockPayees::GetBestPayee -- ERROR: couldn't find any payee\n");
        return false;
    }

    int nVotes = -1;
    BOOST_FOREACH(CApollonnodePayee & payee, vecPayees)
    {
        if (payee.GetVoteCount() > nVotes) {
            payeeRet = payee.GetPayee();
            nVotes = payee.GetVoteCount();
        }
    }

    return (nVotes > -1);
}

bool CApollonnodeBlockPayees::HasPayeeWithVotes(CScript payeeIn, int nVotesReq) {
    LOCK(cs_vecPayees);

    BOOST_FOREACH(CApollonnodePayee & payee, vecPayees)
    {
        if (payee.GetVoteCount() >= nVotesReq && payee.GetPayee() == payeeIn) {
            return true;
        }
    }

//    LogPrint("mnpayments", "CApollonnodeBlockPayees::HasPayeeWithVotes -- ERROR: couldn't find any payee with %d+ votes\n", nVotesReq);
    return false;
}

bool CApollonnodeBlockPayees::IsTransactionValid(const CTransaction &txNew, bool fMTP,int nHeight) {
    LOCK(cs_vecPayees);

    int nMaxSignatures = 0;
    std::string strPayeesPossible = "";


    CAmount nApollonnodePayment = GetApollonnodePayment(Params().GetConsensus(), fMTP,nHeight);

    //require at least MNPAYMENTS_SIGNATURES_REQUIRED signatures

    BOOST_FOREACH(CApollonnodePayee & payee, vecPayees)
    {
        if (payee.GetVoteCount() >= nMaxSignatures) {
            nMaxSignatures = payee.GetVoteCount();
        }
    }
    LogPrintf("nmaxsig = %s \n",nMaxSignatures);
    // if we don't have at least MNPAYMENTS_SIGNATURES_REQUIRED signatures on a payee, approve whichever is the longest chain
    if (nMaxSignatures < MNPAYMENTS_SIGNATURES_REQUIRED) return true;

    bool hasValidPayee = false;

    BOOST_FOREACH(CApollonnodePayee & payee, vecPayees)
    {
        if (payee.GetVoteCount() >= MNPAYMENTS_SIGNATURES_REQUIRED) {
            hasValidPayee = true;

            BOOST_FOREACH(CTxOut txout, txNew.vout) {
                if (payee.GetPayee() == txout.scriptPubKey && nApollonnodePayment == txout.nValue) {
                    LogPrint("mnpayments", "CApollonnodeBlockPayees::IsTransactionValid -- Found required payment\n");
                    return true;
                }
            }

            CTxDestination address1;
            ExtractDestination(payee.GetPayee(), address1);
            CBitcoinAddress address2(address1);

            if (strPayeesPossible == "") {
                strPayeesPossible = address2.ToString();
            } else {
                strPayeesPossible += "," + address2.ToString();
            }
        }
    }

    LogPrintf("CApollonnodeBlockPayees::IsTransactionValid -- ERROR: Missing required payment, possible payees: '%s', amount: %f XAP\n", strPayeesPossible, (float) nApollonnodePayment / COIN);
    return false;
}

std::string CApollonnodeBlockPayees::GetRequiredPaymentsString() {
    LOCK(cs_vecPayees);

    std::string strRequiredPayments = "Unknown";

    BOOST_FOREACH(CApollonnodePayee & payee, vecPayees)
    {
        CTxDestination address1;
        ExtractDestination(payee.GetPayee(), address1);
        CBitcoinAddress address2(address1);

        if (strRequiredPayments != "Unknown") {
            strRequiredPayments += ", " + address2.ToString() + ":" + boost::lexical_cast<std::string>(payee.GetVoteCount());
        } else {
            strRequiredPayments = address2.ToString() + ":" + boost::lexical_cast<std::string>(payee.GetVoteCount());
        }
    }

    return strRequiredPayments;
}

std::string CApollonnodePayments::GetRequiredPaymentsString(int nBlockHeight) {
    LOCK(cs_mapApollonnodeBlocks);

    if (mapApollonnodeBlocks.count(nBlockHeight)) {
        return mapApollonnodeBlocks[nBlockHeight].GetRequiredPaymentsString();
    }

    return "Unknown";
}

bool CApollonnodePayments::IsTransactionValid(const CTransaction &txNew, int nBlockHeight, bool fMTP) {
    LOCK(cs_mapApollonnodeBlocks);

    if (mapApollonnodeBlocks.count(nBlockHeight)) {
        return mapApollonnodeBlocks[nBlockHeight].IsTransactionValid(txNew, fMTP,nBlockHeight);
    }

    return true;
}

void CApollonnodePayments::CheckAndRemove() {
    if (!pCurrentBlockApollon) return;

    LOCK2(cs_mapApollonnodeBlocks, cs_mapApollonnodePaymentVotes);

    int nLimit = GetStorageLimit();

    std::map<uint256, CApollonnodePaymentVote>::iterator it = mapApollonnodePaymentVotes.begin();
    while (it != mapApollonnodePaymentVotes.end()) {
        CApollonnodePaymentVote vote = (*it).second;

        if (pCurrentBlockApollon->nHeight - vote.nBlockHeight > nLimit) {
            LogPrint("mnpayments", "CApollonnodePayments::CheckAndRemove -- Removing old Apollonnode payment: nBlockHeight=%d\n", vote.nBlockHeight);
            mapApollonnodePaymentVotes.erase(it++);
            mapApollonnodeBlocks.erase(vote.nBlockHeight);
        } else {
            ++it;
        }
    }
    LogPrintf("CApollonnodePayments::CheckAndRemove -- %s\n", ToString());
}

bool CApollonnodePaymentVote::IsValid(CNode *pnode, int nValidationHeight, std::string &strError) {
    CApollonnode *pmn = mnodeman.Find(vinApollonnode);

    if (!pmn) {
        strError = strprintf("Unknown Apollonnode: prevout=%s", vinApollonnode.prevout.ToStringShort());
        // Only ask if we are already synced and still have no idea about that Apollonnode
        if (apollonnodeSync.IsApollonnodeListSynced()) {
            mnodeman.AskForMN(pnode, vinApollonnode);
        }

        return false;
    }

    int nMinRequiredProtocol;
    if (nBlockHeight >= nValidationHeight) {
        // new votes must comply SPORK_10_APOLLONNODE_PAY_UPDATED_NODES rules
        nMinRequiredProtocol = mnpayments.GetMinApollonnodePaymentsProto();
    } else {
        // allow non-updated apollonnodes for old blocks
        nMinRequiredProtocol = MIN_APOLLONNODE_PAYMENT_PROTO_VERSION_1;
    }

    if (pmn->nProtocolVersion < nMinRequiredProtocol) {
        strError = strprintf("Apollonnode protocol is too old: nProtocolVersion=%d, nMinRequiredProtocol=%d", pmn->nProtocolVersion, nMinRequiredProtocol);
        return false;
    }

    // Only apollonnodes should try to check apollonnode rank for old votes - they need to pick the right winner for future blocks.
    // Regular clients (miners included) need to verify apollonnode rank for future block votes only.
    if (!fApollonNode && nBlockHeight < nValidationHeight) return true;

    int nRank = mnodeman.GetApollonnodeRank(vinApollonnode, nBlockHeight - 101, nMinRequiredProtocol, false);

    if (nRank == -1) {
        LogPrint("mnpayments", "CApollonnodePaymentVote::IsValid -- Can't calculate rank for apollonnode %s\n",
                 vinApollonnode.prevout.ToStringShort());
        return false;
    }

    if (nRank > MNPAYMENTS_SIGNATURES_TOTAL) {
        // It's common to have apollonnodes mistakenly think they are in the top 10
        // We don't want to print all of these messages in normal mode, debug mode should print though
        strError = strprintf("Apollonnode is not in the top %d (%d)", MNPAYMENTS_SIGNATURES_TOTAL, nRank);
        // Only ban for new mnw which is out of bounds, for old mnw MN list itself might be way too much off
        if (nRank > MNPAYMENTS_SIGNATURES_TOTAL * 2 && nBlockHeight > nValidationHeight) {
            strError = strprintf("Apollonnode is not in the top %d (%d)", MNPAYMENTS_SIGNATURES_TOTAL * 2, nRank);
            LogPrintf("CApollonnodePaymentVote::IsValid -- Error: %s\n", strError);
            Misbehaving(pnode->GetId(), 20);
        }
        // Still invalid however
        return false;
    }

    return true;
}

bool CApollonnodePayments::ProcessBlock(int nBlockHeight) {

    // DETERMINE IF WE SHOULD BE VOTING FOR THE NEXT PAYEE

    if (fLiteMode || !fApollonNode) {
        return false;
    }

    // We have little chances to pick the right winner if winners list is out of sync
    // but we have no choice, so we'll try. However it doesn't make sense to even try to do so
    // if we have not enough data about apollonnodes.
    if (!apollonnodeSync.IsApollonnodeListSynced()) {
        return false;
    }

    int nRank = mnodeman.GetApollonnodeRank(activeApollonnode.vin, nBlockHeight - 101, GetMinApollonnodePaymentsProto(), false);

    if (nRank == -1) {
        LogPrint("mnpayments", "CApollonnodePayments::ProcessBlock -- Unknown Apollonnode\n");
        return false;
    }

    if (nRank > MNPAYMENTS_SIGNATURES_TOTAL) {
        LogPrint("mnpayments", "CApollonnodePayments::ProcessBlock -- Apollonnode not in the top %d (%d)\n", MNPAYMENTS_SIGNATURES_TOTAL, nRank);
        return false;
    }

    // LOCATE THE NEXT APOLLONNODE WHICH SHOULD BE PAID

    LogPrintf("CApollonnodePayments::ProcessBlock -- Start: nBlockHeight=%d, apollonnode=%s\n", nBlockHeight, activeApollonnode.vin.prevout.ToStringShort());

    // pay to the oldest MN that still had no payment but its input is old enough and it was active long enough
    int nCount = 0;
    CApollonnode *pmn = mnodeman.GetNextApollonnodeInQueueForPayment(nBlockHeight, true, nCount);

    if (pmn == NULL) {
        LogPrintf("CApollonnodePayments::ProcessBlock -- ERROR: Failed to find apollonnode to pay\n");
        return false;
    }

    LogPrintf("CApollonnodePayments::ProcessBlock -- Apollonnode found by GetNextApollonnodeInQueueForPayment(): %s\n", pmn->vin.prevout.ToStringShort());


    CScript payee = GetScriptForDestination(pmn->pubKeyCollateralAddress.GetID());

    CApollonnodePaymentVote voteNew(activeApollonnode.vin, nBlockHeight, payee);

    CTxDestination address1;
    ExtractDestination(payee, address1);
    CBitcoinAddress address2(address1);

    // SIGN MESSAGE TO NETWORK WITH OUR APOLLONNODE KEYS

    if (voteNew.Sign()) {
        if (AddPaymentVote(voteNew)) {
            voteNew.Relay();
            return true;
        }
    }

    return false;
}

void CApollonnodePaymentVote::Relay() {
    // do not relay until synced
    if (!apollonnodeSync.IsWinnersListSynced()) {
        LogPrint("apollonnode", "CApollonnodePaymentVote::Relay - apollonnodeSync.IsWinnersListSynced() not sync\n");
        return;
    }
    CInv inv(MSG_APOLLONNODE_PAYMENT_VOTE, GetHash());
    RelayInv(inv);
}

bool CApollonnodePaymentVote::CheckSignature(const CPubKey &pubKeyApollonnode, int nValidationHeight, int &nDos) {
    // do not ban by default
    nDos = 0;

    std::string strMessage = vinApollonnode.prevout.ToStringShort() +
                             boost::lexical_cast<std::string>(nBlockHeight) +
                             ScriptToAsmStr(payee);

    std::string strError = "";
    if (!darkSendSigner.VerifyMessage(pubKeyApollonnode, vchSig, strMessage, strError)) {
        // Only ban for future block vote when we are already synced.
        // Otherwise it could be the case when MN which signed this vote is using another key now
        // and we have no idea about the old one.
        if (apollonnodeSync.IsApollonnodeListSynced() && nBlockHeight > nValidationHeight) {
            nDos = 20;
        }
        return error("CApollonnodePaymentVote::CheckSignature -- Got bad Apollonnode payment signature, apollonnode=%s, error: %s", vinApollonnode.prevout.ToStringShort().c_str(), strError);
    }

    return true;
}

std::string CApollonnodePaymentVote::ToString() const {
    std::ostringstream info;

    info << vinApollonnode.prevout.ToStringShort() <<
         ", " << nBlockHeight <<
         ", " << ScriptToAsmStr(payee) <<
         ", " << (int) vchSig.size();

    return info.str();
}

// Send only votes for future blocks, node should request every other missing payment block individually
void CApollonnodePayments::Sync(CNode *pnode) {
    LOCK(cs_mapApollonnodeBlocks);

    if (!pCurrentBlockApollon) return;

    int nInvCount = 0;

    for (int h = pCurrentBlockApollon->nHeight; h < pCurrentBlockApollon->nHeight + 20; h++) {
        if (mapApollonnodeBlocks.count(h)) {
            BOOST_FOREACH(CApollonnodePayee & payee, mapApollonnodeBlocks[h].vecPayees)
            {
                std::vector <uint256> vecVoteHashes = payee.GetVoteHashes();
                BOOST_FOREACH(uint256 & hash, vecVoteHashes)
                {
                    if (!HasVerifiedPaymentVote(hash)) continue;
                    pnode->PushInventory(CInv(MSG_APOLLONNODE_PAYMENT_VOTE, hash));
                    nInvCount++;
                }
            }
        }
    }

    LogPrintf("CApollonnodePayments::Sync -- Sent %d votes to peer %d\n", nInvCount, pnode->id);
    pnode->PushMessage(NetMsgType::SYNCSTATUSCOUNT, APOLLONNODE_SYNC_MNW, nInvCount);
}

// Request low data/unknown payment blocks in batches directly from some node instead of/after preliminary Sync.
void CApollonnodePayments::RequestLowDataPaymentBlocks(CNode *pnode) {
    if (!pCurrentBlockApollon) return;

    LOCK2(cs_main, cs_mapApollonnodeBlocks);

    std::vector <CInv> vToFetch;
    int nLimit = GetStorageLimit();

    const CBlockApollon *papollon = pCurrentBlockApollon;

    while (pCurrentBlockApollon->nHeight - papollon->nHeight < nLimit) {
        if (!mapApollonnodeBlocks.count(papollon->nHeight)) {
            // We have no idea about this block height, let's ask
            vToFetch.push_back(CInv(MSG_APOLLONNODE_PAYMENT_BLOCK, papollon->GetBlockHash()));
            // We should not violate GETDATA rules
            if (vToFetch.size() == MAX_INV_SZ) {
                LogPrintf("CApollonnodePayments::SyncLowDataPaymentBlocks -- asking peer %d for %d blocks\n", pnode->id, MAX_INV_SZ);
                pnode->PushMessage(NetMsgType::GETDATA, vToFetch);
                // Start filling new batch
                vToFetch.clear();
            }
        }
        if (!papollon->pprev) break;
        papollon = papollon->pprev;
    }

    std::map<int, CApollonnodeBlockPayees>::iterator it = mapApollonnodeBlocks.begin();

    while (it != mapApollonnodeBlocks.end()) {
        int nTotalVotes = 0;
        bool fFound = false;
        BOOST_FOREACH(CApollonnodePayee & payee, it->second.vecPayees)
        {
            if (payee.GetVoteCount() >= MNPAYMENTS_SIGNATURES_REQUIRED) {
                fFound = true;
                break;
            }
            nTotalVotes += payee.GetVoteCount();
        }
        // A clear winner (MNPAYMENTS_SIGNATURES_REQUIRED+ votes) was found
        // or no clear winner was found but there are at least avg number of votes
        if (fFound || nTotalVotes >= (MNPAYMENTS_SIGNATURES_TOTAL + MNPAYMENTS_SIGNATURES_REQUIRED) / 2) {
            // so just move to the next block
            ++it;
            continue;
        }
        // DEBUG
//        DBG (
//            // Let's see why this failed
//            BOOST_FOREACH(CApollonnodePayee& payee, it->second.vecPayees) {
//                CTxDestination address1;
//                ExtractDestination(payee.GetPayee(), address1);
//                CBitcoinAddress address2(address1);
//                printf("payee %s votes %d\n", address2.ToString().c_str(), payee.GetVoteCount());
//            }
//            printf("block %d votes total %d\n", it->first, nTotalVotes);
//        )
        // END DEBUG
        // Low data block found, let's try to sync it
        uint256 hash;
        if (GetBlockHash(hash, it->first)) {
            vToFetch.push_back(CInv(MSG_APOLLONNODE_PAYMENT_BLOCK, hash));
        }
        // We should not violate GETDATA rules
        if (vToFetch.size() == MAX_INV_SZ) {
            LogPrintf("CApollonnodePayments::SyncLowDataPaymentBlocks -- asking peer %d for %d payment blocks\n", pnode->id, MAX_INV_SZ);
            pnode->PushMessage(NetMsgType::GETDATA, vToFetch);
            // Start filling new batch
            vToFetch.clear();
        }
        ++it;
    }
    // Ask for the rest of it
    if (!vToFetch.empty()) {
        LogPrintf("CApollonnodePayments::SyncLowDataPaymentBlocks -- asking peer %d for %d payment blocks\n", pnode->id, vToFetch.size());
        pnode->PushMessage(NetMsgType::GETDATA, vToFetch);
    }
}

std::string CApollonnodePayments::ToString() const {
    std::ostringstream info;

    info << "Votes: " << (int) mapApollonnodePaymentVotes.size() <<
         ", Blocks: " << (int) mapApollonnodeBlocks.size();

    return info.str();
}

bool CApollonnodePayments::IsEnoughData() {
    float nAverageVotes = (MNPAYMENTS_SIGNATURES_TOTAL + MNPAYMENTS_SIGNATURES_REQUIRED) / 2;
    int nStorageLimit = GetStorageLimit();
    return GetBlockCount() > nStorageLimit && GetVoteCount() > nStorageLimit * nAverageVotes;
}

int CApollonnodePayments::GetStorageLimit() {
    return std::max(int(mnodeman.size() * nStorageCoeff), nMinBlocksToStore);
}

void CApollonnodePayments::UpdatedBlockTip(const CBlockApollon *papollon) {
    pCurrentBlockApollon = papollon;
    LogPrint("mnpayments", "CApollonnodePayments::UpdatedBlockTip -- pCurrentBlockApollon->nHeight=%d\n", pCurrentBlockApollon->nHeight);
    
    ProcessBlock(papollon->nHeight + 5);
}
