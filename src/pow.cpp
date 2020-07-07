// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pow.h"
#include "main.h"
#include "arith_uint256.h"
#include "chain.h"
#include "primitives/block.h"
#include "consensus/consensus.h"
#include "uint256.h"
#include <iostream>
#include "util.h"
#include "chainparams.h"
#include "libzerocoin/bitcoin_bignum/bignum.h"
#include "utilstrencodings.h"
#include "fixed.h"
static CBigNum bnProofOfWorkLimit(~arith_uint256(0) >> 8);

double GetDifficultyHelper(unsigned int nBits) {
    int nShift = (nBits >> 24) & 0xff;
    double dDiff = (double) 0x0000ffff / (double) (nBits & 0x00ffffff);

    while (nShift < 29) {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29) {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}

unsigned int DarkGravityWave(const CBlockApollon* papollonLast, const Consensus::Params& params, bool fProofOfStake) {
    /* current difficulty formula, veil - DarkGravity v3, written by Evan Duffield - evan@dash.org */
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);

    const CBlockApollon *papollon = papollonLast;
    const CBlockApollon* papollonLastMatchingProof = nullptr;
    arith_uint256 bnPastTargetAvg = 0;
    // make sure we have at least (nPastBlocks + 1) blocks, otherwise just return powLimit
    if (!papollonLast || papollonLast->nHeight < params.nDgwPastBlocks) {
        return bnPowLimit.GetCompact();
    }
    
    unsigned int nCountBlocks = 0;
    while (nCountBlocks < params.nDgwPastBlocks) {
        // Ran out of blocks, return pow limit
        if (!papollon)
            return bnPowLimit.GetCompact();

        // Only consider PoW or PoS blocks but not both
        if (papollon->IsProofOfStake() != fProofOfStake) {
            papollon = papollon->pprev;
            continue;
        } else if (!papollonLastMatchingProof) {
            papollonLastMatchingProof = papollon;
        }

        arith_uint256 bnTarget = arith_uint256().SetCompact(papollon->nBits);
        bnPastTargetAvg = (bnPastTargetAvg * nCountBlocks + bnTarget) / (nCountBlocks + 1);

        if (++nCountBlocks != params.nDgwPastBlocks)
            papollon = papollon->pprev;
    }

    arith_uint256 bnNew(bnPastTargetAvg);

    //Should only happen on the first PoS block
    if (papollonLastMatchingProof)
        papollonLastMatchingProof = papollonLast;

    int64_t nActualTimespan = papollonLastMatchingProof->GetBlockTime() - papollon->GetBlockTime();
    int64_t nTargetTimespan = params.nDgwPastBlocks * params.nPowTargetSpacing;

    if (nActualTimespan < nTargetTimespan/3)
        nActualTimespan = nTargetTimespan/3;
    if (nActualTimespan > nTargetTimespan*3)
        nActualTimespan = nTargetTimespan*3;

    // Retarget
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;

    if (bnNew > bnPowLimit) {
        bnNew = bnPowLimit;
    }

    return bnNew.GetCompact();
}

// Apollon GetNextWorkRequired
unsigned int GetNextWorkRequired(const CBlockApollon *papollonLast, const CBlockHeader *pblock, const Consensus::Params &params,bool fProofOfStake) {
    assert(papollonLast != nullptr);

    // Special rule for regtest: we never retarget.
    if (params.fPowNoRetargeting) {
        return papollonLast->nBits;
    }

    return DarkGravityWave(papollonLast, params,fProofOfStake);

}

unsigned int CalculateNextWorkRequired(const CBlockApollon *papollonLast, int64_t nFirstBlockTime, const Consensus::Params &params) {
    if (params.fPowNoRetargeting)
        return papollonLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = papollonLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan / 4)
        nActualTimespan = params.nPowTargetTimespan / 4;
    if (nActualTimespan > params.nPowTargetTimespan * 4)
        nActualTimespan = params.nPowTargetTimespan * 4;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    bnNew.SetCompact(papollonLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params &params) {
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;
    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);
    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit)){
        return false;
    }
        // Check proof of work matches claimed amount
        if (UintToArith256(hash) > bnTarget){
           return error("CheckProofOfWork() : hash doesn't match nBits\n");
        }
    return true;
}