/**
 * @file utilsbitcoin.cpp
 *
 * This file contains certain helpers to access information about Bitcoin.
 */

#include "chain.h"
#include "chainparams.h"
#include "main.h"
#include "sync.h"

#include <stdint.h>
#include <string>

namespace elysium
{
/**
 * @return The current chain length.
 */
int GetHeight()
{
    LOCK(cs_main);
    return chainActive.Height();
}

/**
 * @return The timestamp of the latest block.
 */
uint32_t GetLatestBlockTime()
{
    LOCK(cs_main);
    if (chainActive.Tip())
        return chainActive.Tip()->GetBlockTime();
    else
        return Params().GenesisBlock().nTime;
}

/**
 * @return The CBlockApollon, or NULL, if the block isn't available.
 */
CBlockApollon* GetBlockApollon(const uint256& hash)
{
    CBlockApollon* pBlockApollon = NULL;
    LOCK(cs_main);
    BlockMap::const_iterator it = mapBlockApollon.find(hash);
    if (it != mapBlockApollon.end()) {
        pBlockApollon = it->second;
    }

    return pBlockApollon;
}

bool MainNet()
{
    return Params().NetworkIDString() == "main";
}

bool TestNet()
{
    return Params().NetworkIDString() == "test";
}

bool RegTest()
{
    return Params().NetworkIDString() == "regtest";
}

bool UnitTest()
{
    return Params().NetworkIDString() == "unittest";
}

bool isNonMainNet()
{
    return !MainNet() && !UnitTest();
}


} // namespace elysium
