// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_ADDRESSAPOLLON_H
#define BITCOIN_ADDRESSAPOLLON_H

#include "uint256.h"
#include "amount.h"
#include "addresstype.h"

struct CMempoolAddressDelta
{
    int64_t time;
    CAmount amount;
    uint256 prevhash;
    unsigned int prevout;

    CMempoolAddressDelta(int64_t t, CAmount a, uint256 hash, unsigned int out) {
        time = t;
        amount = a;
        prevhash = hash;
        prevout = out;
    }

    CMempoolAddressDelta(int64_t t, CAmount a) {
        time = t;
        amount = a;
        prevhash.SetNull();
        prevout = 0;
    }
};

struct CMempoolAddressDeltaKey
{
    AddressType type;
    uint160 addressBytes;
    uint256 txhash;
    unsigned int apollon;
    int spending;

    CMempoolAddressDeltaKey(AddressType addressType, uint160 addressHash, uint256 hash, unsigned int i, int s) {
        type = addressType;
        addressBytes = addressHash;
        txhash = hash;
        apollon = i;
        spending = s;
    }

    CMempoolAddressDeltaKey(AddressType addressType, uint160 addressHash) {
        type = addressType;
        addressBytes = addressHash;
        txhash.SetNull();
        apollon = 0;
        spending = 0;
    }
};

struct CMempoolAddressDeltaKeyCompare
{
    bool operator()(const CMempoolAddressDeltaKey& a, const CMempoolAddressDeltaKey& b) const {
        if (a.type == b.type) {
            if (a.addressBytes == b.addressBytes) {
                if (a.txhash == b.txhash) {
                    if (a.apollon == b.apollon) {
                        return a.spending < b.spending;
                    } else {
                        return a.apollon < b.apollon;
                    }
                } else {
                    return a.txhash < b.txhash;
                }
            } else {
                return a.addressBytes < b.addressBytes;
            }
        } else {
            return a.type < b.type;
        }
    }
};

#endif // BITCOIN_ADDRESSAPOLLON_H