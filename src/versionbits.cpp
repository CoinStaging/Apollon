// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "versionbits.h"

#include "consensus/params.h"

const struct BIP9DeploymentInfo VersionBitsDeploymentInfo[Consensus::MAX_VERSION_BITS_DEPLOYMENTS] = {
    {
        /*.name =*/ "testdummy",
        /*.gbt_force =*/ true,
    },
    {
        /*.name =*/ "csv",
        /*.gbt_force =*/ true,
    },
    {
        /*.name =*/ "segwit",
        /*.gbt_force =*/ false,
    }
};

ThresholdState AbstractThresholdConditionChecker::GetStateFor(const CBlockApollon* papollonPrev, const Consensus::Params& params, ThresholdConditionCache& cache) const
{
    int nPeriod = Period(params);
    int nThreshold = Threshold(params);
    int64_t nTimeStart = BeginTime(params);
    int64_t nTimeTimeout = EndTime(params);

    // A block's state is always the same as that of the first of its period, so it is computed based on a papollonPrev whose height equals a multiple of nPeriod - 1.
    if (papollonPrev != NULL) {
        papollonPrev = papollonPrev->GetAncestor(papollonPrev->nHeight - ((papollonPrev->nHeight + 1) % nPeriod));
    }

    // Walk backwards in steps of nPeriod to find a papollonPrev whose information is known
    std::vector<const CBlockApollon*> vToCompute;
    while (cache.count(papollonPrev) == 0) {
        if (papollonPrev == NULL) {
            // The genesis block is by definition defined.
            cache[papollonPrev] = THRESHOLD_DEFINED;
            break;
        }
        if (papollonPrev->GetMedianTimePast() < nTimeStart) {
            // Optimization: don't recompute down further, as we know every earlier block will be before the start time
            cache[papollonPrev] = THRESHOLD_DEFINED;
            break;
        }
        vToCompute.push_back(papollonPrev);
        papollonPrev = papollonPrev->GetAncestor(papollonPrev->nHeight - nPeriod);
    }

    // At this point, cache[papollonPrev] is known
    assert(cache.count(papollonPrev));
    ThresholdState state = cache[papollonPrev];

    // Now walk forward and compute the state of descendants of papollonPrev
    while (!vToCompute.empty()) {
        ThresholdState stateNext = state;
        papollonPrev = vToCompute.back();
        vToCompute.pop_back();

        switch (state) {
            case THRESHOLD_DEFINED: {
                if (papollonPrev->GetMedianTimePast() >= nTimeTimeout) {
                    stateNext = THRESHOLD_FAILED;
                } else if (papollonPrev->GetMedianTimePast() >= nTimeStart) {
                    stateNext = THRESHOLD_STARTED;
                }
                break;
            }
            case THRESHOLD_STARTED: {
                if (papollonPrev->GetMedianTimePast() >= nTimeTimeout) {
                    stateNext = THRESHOLD_FAILED;
                    break;
                }
                // We need to count
                const CBlockApollon* papollonCount = papollonPrev;
                int count = 0;
                for (int i = 0; i < nPeriod; i++) {
                    if (Condition(papollonCount, params)) {
                        count++;
                    }
                    papollonCount = papollonCount->pprev;
                }
                if (count >= nThreshold) {
                    stateNext = THRESHOLD_LOCKED_IN;
                }
                break;
            }
            case THRESHOLD_LOCKED_IN: {
                // Always progresses into ACTIVE.
                stateNext = THRESHOLD_ACTIVE;
                break;
            }
            case THRESHOLD_FAILED:
            case THRESHOLD_ACTIVE: {
                // Nothing happens, these are terminal states.
                break;
            }
        }
        cache[papollonPrev] = state = stateNext;
    }

    return state;
}

namespace
{
/**
 * Class to implement versionbits logic.
 */
class VersionBitsConditionChecker : public AbstractThresholdConditionChecker {
private:
    const Consensus::DeploymentPos id;

protected:
    int64_t BeginTime(const Consensus::Params& params) const { return params.vDeployments[id].nStartTime; }
    int64_t EndTime(const Consensus::Params& params) const { return params.vDeployments[id].nTimeout; }
    int Period(const Consensus::Params& params) const { return params.nMinerConfirmationWindow; }
    int Threshold(const Consensus::Params& params) const { return params.nRuleChangeActivationThreshold; }

    bool Condition(const CBlockApollon* papollon, const Consensus::Params& params) const
    {
        return (((papollon->nVersion & VERSIONBITS_TOP_MASK) == VERSIONBITS_TOP_BITS) && (papollon->nVersion & Mask(params)) != 0);
    }

public:
    VersionBitsConditionChecker(Consensus::DeploymentPos id_) : id(id_) {}
    uint32_t Mask(const Consensus::Params& params) const { return ((uint32_t)1) << params.vDeployments[id].bit; }
};

}

ThresholdState VersionBitsState(const CBlockApollon* papollonPrev, const Consensus::Params& params, Consensus::DeploymentPos pos, VersionBitsCache& cache)
{
    return VersionBitsConditionChecker(pos).GetStateFor(papollonPrev, params, cache.caches[pos]);
}

uint32_t VersionBitsMask(const Consensus::Params& params, Consensus::DeploymentPos pos)
{
    return VersionBitsConditionChecker(pos).Mask(params);
}

void VersionBitsCache::Clear()
{
    for (unsigned int d = 0; d < Consensus::MAX_VERSION_BITS_DEPLOYMENTS; d++) {
        caches[d].clear();
    }
}
