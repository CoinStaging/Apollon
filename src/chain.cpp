// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chain.h"

using namespace std;

/**
 * CChain implementation
 */
void CChain::SetTip(CBlockApollon *papollon) {
    if (papollon == NULL) {
        vChain.clear();
        return;
    }
    vChain.resize(papollon->nHeight + 1);
    while (papollon && vChain[papollon->nHeight] != papollon) {
        vChain[papollon->nHeight] = papollon;
        papollon = papollon->pprev;
    }
}

CBlockLocator CChain::GetLocator(const CBlockApollon *papollon) const {
    int nStep = 1;
    std::vector<uint256> vHave;
    vHave.reserve(32);

    if (!papollon)
        papollon = Tip();
    while (papollon) {
        vHave.push_back(papollon->GetBlockHash());
        // Stop when we have added the genesis block.
        if (papollon->nHeight == 0)
            break;
        // Exponentially larger steps back, plus the genesis block.
        int nHeight = std::max(papollon->nHeight - nStep, 0);
        if (Contains(papollon)) {
            // Use O(1) CChain apollon if possible.
            papollon = (*this)[nHeight];
        } else {
            // Otherwise, use O(log n) skiplist.
            papollon = papollon->GetAncestor(nHeight);
        }
        if (vHave.size() > 10)
            nStep *= 2;
    }

    return CBlockLocator(vHave);
}

const CBlockApollon *CChain::FindFork(const CBlockApollon *papollon) const {
    if (papollon == NULL) {
        return NULL;
    }
    if (papollon->nHeight > Height())
        papollon = papollon->GetAncestor(Height());
    while (papollon && !Contains(papollon))
        papollon = papollon->pprev;
    return papollon;
}

/** Turn the lowest '1' bit in the binary representation of a number into a '0'. */
int static inline InvertLowestOne(int n) { return n & (n - 1); }

/** Compute what height to jump back to with the CBlockApollon::pskip pointer. */
int static inline GetSkipHeight(int height) {
    if (height < 2)
        return 0;

    // Determine which height to jump back to. Any number strictly lower than height is acceptable,
    // but the following expression seems to perform well in simulations (max 110 steps to go back
    // up to 2**18 blocks).
    return (height & 1) ? InvertLowestOne(InvertLowestOne(height - 1)) + 1 : InvertLowestOne(height);
}

CBlockApollon* CBlockApollon::GetAncestor(int height)
{
    if (height > nHeight || height < 0)
        return NULL;

    CBlockApollon* papollonWalk = this;
    int heightWalk = nHeight;
    while (heightWalk > height) {
        int heightSkip = GetSkipHeight(heightWalk);
        int heightSkipPrev = GetSkipHeight(heightWalk - 1);
        if (papollonWalk->pskip != NULL &&
            (heightSkip == height ||
             (heightSkip > height && !(heightSkipPrev < heightSkip - 2 &&
                                       heightSkipPrev >= height)))) {
            // Only follow pskip if pprev->pskip isn't better than pskip->pprev.
            papollonWalk = papollonWalk->pskip;
            heightWalk = heightSkip;
        } else {
            //assert(papollonWalk->pprev);
            papollonWalk = papollonWalk->pprev;
            heightWalk--;
        }
    }
    return papollonWalk;
}

const CBlockApollon* CBlockApollon::GetAncestor(int height) const
{
    return const_cast<CBlockApollon*>(this)->GetAncestor(height);
}

void CBlockApollon::BuildSkip()
{
    if (pprev)
        pskip = pprev->GetAncestor(GetSkipHeight(nHeight));
}

const CBlockApollon* GetLastBlockApollon(const CBlockApollon* papollon, bool fProofOfStake)
{
    while (papollon && papollon->pprev && (papollon->IsProofOfStake() != fProofOfStake))
        papollon = papollon->pprev;
    return papollon;
}

arith_uint256 GetBlockProof(const CBlockApollon& block)
{
    arith_uint256 bnTarget;
    bool fNegative;
    bool fOverflow;
    bnTarget.SetCompact(block.nBits, &fNegative, &fOverflow);
    if (fNegative || fOverflow || bnTarget == 0)
        return 0;
    // We need to compute 2**256 / (bnTarget+1), but we can't represent 2**256
    // as it's too large for a arith_uint256. However, as 2**256 is at least as large
    // as bnTarget+1, it is equal to ((2**256 - bnTarget - 1) / (bnTarget+1)) + 1,
    // or ~bnTarget / (nTarget+1) + 1.
    return (~bnTarget / (bnTarget + 1)) + 1;
}

int64_t GetBlockProofEquivalentTime(const CBlockApollon& to, const CBlockApollon& from, const CBlockApollon& tip, const Consensus::Params& params)
{
    arith_uint256 r;
    int sign = 1;
    if (to.nChainWork > from.nChainWork) {
        r = to.nChainWork - from.nChainWork;
    } else {
        r = from.nChainWork - to.nChainWork;
        sign = -1;
    }
    r = r * arith_uint256(params.nPowTargetSpacing) / GetBlockProof(tip);
    if (r.bits() > 63) {
        return sign * std::numeric_limits<int64_t>::max();
    }
    return sign * r.GetLow64();
}
