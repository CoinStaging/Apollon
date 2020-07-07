// Copyright (c) 2020 The Apollon Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ZCOIN_MINTPOOL_H
#define ZCOIN_MINTPOOL_H

#include <map>
#include <list>

#include "primitives/zerocoin.h"
#include "libzerocoin/bitcoin_bignum/bignum.h"
#include "uint256.h"

typedef std::tuple<uint160, CKeyID, int32_t> MintPoolEntry;

/**
 * The MintPool only contains mint seed values that have not been added to the blockchain yet.
 * When a mint is generated by the wallet, or found while syncing, the mint will be removed
 * from the MintPool.
 *
 * The MintPool provides a convenient way to check whether mints in the blockchain belong to a
 * wallet's deterministic seed.
 */
class CMintPool : public std::map<uint256, MintPoolEntry> //hashPubcoin mapped to (hashSeedMaster, seedId, count)
{

public:
    CMintPool();
    void Add(pair<uint256, MintPoolEntry> pMint, bool fVerbose = false);
    void List(list<pair<uint256, MintPoolEntry>>& listMints);
    void Reset();
    bool Get(int32_t nCount, uint160 hashSeedMaster, pair<uint256, MintPoolEntry>& result);
};

#endif // ZCOIN_MINTPOOL_H
