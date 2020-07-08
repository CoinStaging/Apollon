// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SPENTAPOLLON_H
#define BITCOIN_SPENTAPOLLON_H

#include "uint256.h"
#include "amount.h"
#include "script/script.h"
#include "addresstype.h"

struct CSpentApollonKey {
    uint256 txid;
    unsigned int outputApollon;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(txid);
        READWRITE(outputApollon);
    }

    CSpentApollonKey(uint256 t, unsigned int i) {
        txid = t;
        outputApollon = i;
    }

    CSpentApollonKey() {
        SetNull();
    }

    void SetNull() {
        txid.SetNull();
        outputApollon = 0;
    }

};

struct CSpentApollonValue {
    uint256 txid;
    unsigned int inputApollon;
    int blockHeight;
    CAmount satoshis;
    AddressType addressType;
    uint160 addressHash;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        unsigned int addrType = static_cast<unsigned int>(addressType);
        READWRITE(txid);
        READWRITE(inputApollon);
        READWRITE(blockHeight);
        READWRITE(satoshis);
        READWRITE(addrType);
        READWRITE(addressHash);
        addressType = static_cast<AddressType>(addrType);
    }

    CSpentApollonValue(uint256 t, unsigned int i, int h, CAmount s, AddressType type, uint160 a) {
        txid = t;
        inputApollon = i;
        blockHeight = h;
        satoshis = s;
        addressType = type;
        addressHash = a;
    }

    CSpentApollonValue() {
        SetNull();
    }

    void SetNull() {
        txid.SetNull();
        inputApollon = 0;
        blockHeight = 0;
        satoshis = 0;
        addressType = AddressType::unknown;
        addressHash.SetNull();
    }

    bool IsNull() const {
        return txid.IsNull();
    }
};

struct CSpentApollonKeyCompare
{
    bool operator()(const CSpentApollonKey& a, const CSpentApollonKey& b) const {
        if (a.txid == b.txid) {
            return a.outputApollon < b.outputApollon;
        } else {
            return a.txid < b.txid;
        }
    }
};

struct CTimestampApollonIteratorKey {
    unsigned int timestamp;

    size_t GetSerializeSize(int nType, int nVersion) const {
        return 4;
    }
    template<typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const {
        ser_writedata32be(s, timestamp);
    }
    template<typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion) {
        timestamp = ser_readdata32be(s);
    }

    CTimestampApollonIteratorKey(unsigned int time) {
        timestamp = time;
    }

    CTimestampApollonIteratorKey() {
        SetNull();
    }

    void SetNull() {
        timestamp = 0;
    }
};

struct CTimestampApollonKey {
    unsigned int timestamp;
    uint256 blockHash;

    size_t GetSerializeSize(int nType, int nVersion) const {
        return 36;
    }
    template<typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const {
        ser_writedata32be(s, timestamp);
        blockHash.Serialize(s, nType, nVersion);
    }
    template<typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion) {
        timestamp = ser_readdata32be(s);
        blockHash.Unserialize(s, nType, nVersion);
    }

    CTimestampApollonKey(unsigned int time, uint256 hash) {
        timestamp = time;
        blockHash = hash;
    }

    CTimestampApollonKey() {
        SetNull();
    }

    void SetNull() {
        timestamp = 0;
        blockHash.SetNull();
    }
};

struct CAddressUnspentKey {
    AddressType type;
    uint160 hashBytes;
    uint256 txhash;
    size_t apollon;

    size_t GetSerializeSize(int nType, int nVersion) const {
        return 57;
    }
    template<typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const {
        ser_writedata8(s, static_cast<unsigned int>(type));
        hashBytes.Serialize(s, nType, nVersion);
        txhash.Serialize(s, nType, nVersion);
        ser_writedata32(s, apollon);
    }
    template<typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion) {
        type = static_cast<AddressType>(ser_readdata8(s));
        hashBytes.Unserialize(s, nType, nVersion);
        txhash.Unserialize(s, nType, nVersion);
        apollon = ser_readdata32(s);
    }

    CAddressUnspentKey(AddressType addressType, uint160 addressHash, uint256 txid, size_t apollonValue) {
        type = addressType;
        hashBytes = addressHash;
        txhash = txid;
        apollon = apollonValue;
    }

    CAddressUnspentKey() {
        SetNull();
    }

    void SetNull() {
        type = AddressType::unknown;
        hashBytes.SetNull();
        txhash.SetNull();
        apollon = 0;
    }
};

struct CAddressUnspentValue {
    CAmount satoshis;
    CScript script;
    int blockHeight;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(satoshis);
        READWRITE(*(CScriptBase*)(&script));
        READWRITE(blockHeight);
    }

    CAddressUnspentValue(CAmount sats, CScript scriptPubKey, int height) {
        satoshis = sats;
        script = scriptPubKey;
        blockHeight = height;
    }

    CAddressUnspentValue() {
        SetNull();
    }

    void SetNull() {
        satoshis = -1;
        script.clear();
        blockHeight = 0;
    }

    bool IsNull() const {
        return (satoshis == -1);
    }
};

struct CAddressApollonKey {
    AddressType type;
    uint160 hashBytes;
    int blockHeight;
    unsigned int txapollon;
    uint256 txhash;
    size_t apollon;
    bool spending;

    size_t GetSerializeSize(int nType, int nVersion) const {
        return 66;
    }
    template<typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const {
        ser_writedata8(s, static_cast<unsigned int>(type));
        hashBytes.Serialize(s, nType, nVersion);
        // Heights are stored big-endian for key sorting in LevelDB
        ser_writedata32be(s, blockHeight);
        ser_writedata32be(s, txapollon);
        txhash.Serialize(s, nType, nVersion);
        ser_writedata32(s, apollon);
        char f = spending;
        ser_writedata8(s, f);
    }
    template<typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion) {
        type = static_cast<AddressType>(ser_readdata8(s));
        hashBytes.Unserialize(s, nType, nVersion);
        blockHeight = ser_readdata32be(s);
        txapollon = ser_readdata32be(s);
        txhash.Unserialize(s, nType, nVersion);
        apollon = ser_readdata32(s);
        char f = ser_readdata8(s);
        spending = f;
    }

    CAddressApollonKey(AddressType addressType, uint160 addressHash, int height, int blockapollon,
                     uint256 txid, size_t apollonValue, bool isSpending) {
        type = addressType;
        hashBytes = addressHash;
        blockHeight = height;
        txapollon = blockapollon;
        txhash = txid;
        apollon = apollonValue;
        spending = isSpending;
    }

    CAddressApollonKey() {
        SetNull();
    }

    void SetNull() {
        type = AddressType::unknown;
        hashBytes.SetNull();
        blockHeight = 0;
        txapollon = 0;
        txhash.SetNull();
        apollon = 0;
        spending = false;
    }

};

struct CAddressApollonIteratorKey {
    AddressType type;
    uint160 hashBytes;

    size_t GetSerializeSize(int nType, int nVersion) const {
        return 21;
    }
    template<typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const {
        ser_writedata8(s, static_cast<unsigned int>(type));
        hashBytes.Serialize(s, nType, nVersion);
    }
    template<typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion) {
        type = static_cast<AddressType>(ser_readdata8(s));
        hashBytes.Unserialize(s, nType, nVersion);
    }

    CAddressApollonIteratorKey(AddressType addressType, uint160 addressHash) {
        type = addressType;
        hashBytes = addressHash;
    }

    CAddressApollonIteratorKey() {
        SetNull();
    }

    void SetNull() {
        type = AddressType::unknown;
        hashBytes.SetNull();
    }
};

struct CAddressApollonIteratorHeightKey {
    AddressType type;
    uint160 hashBytes;
    int blockHeight;

    size_t GetSerializeSize(int nType, int nVersion) const {
        return 25;
    }
    template<typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const {
        ser_writedata8(s, static_cast<unsigned int>(type));
        hashBytes.Serialize(s, nType, nVersion);
        ser_writedata32be(s, blockHeight);
    }
    template<typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion) {
        type = static_cast<AddressType>(ser_readdata8(s));
        hashBytes.Unserialize(s, nType, nVersion);
        blockHeight = ser_readdata32be(s);
    }

    CAddressApollonIteratorHeightKey(AddressType addressType, uint160 addressHash, int height) {
        type = addressType;
        hashBytes = addressHash;
        blockHeight = height;
    }

    CAddressApollonIteratorHeightKey() {
        SetNull();
    }

    void SetNull() {
        type = AddressType::unknown;
        hashBytes.SetNull();
        blockHeight = 0;
    }
};


#endif // BITCOIN_SPENTAPOLLON_H
