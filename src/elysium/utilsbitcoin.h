#ifndef ELYSIUM_BITCOIN_H
#define	ELYSIUM_BITCOIN_H

class CBlockApollon;
class uint256;

#include <stdint.h>

namespace elysium
{
/** Returns the current chain length. */
int GetHeight();
/** Returns the timestamp of the latest block. */
uint32_t GetLatestBlockTime();
/** Returns the CBlockApollon for a given block hash, or NULL. */
CBlockApollon* GetBlockApollon(const uint256& hash);

bool MainNet();
bool TestNet();
bool RegTest();
bool UnitTest();
bool isNonMainNet();
}

#endif // ELYSIUM_BITCOIN_H
