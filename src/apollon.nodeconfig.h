
// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SRC_APOLLONNODECONFIG_H_
#define SRC_APOLLONNODECONFIG_H_

#include "fs.h"

#include <univalue.h>

class CApollonnodeConfig;
extern CApollonnodeConfig apollonnodeConfig;

class CApollonnodeConfig
{

public:

    class CApollonnodeEntry {

    private:
        std::string alias;
        std::string ip;
        std::string privKey;
        std::string txHash;
        std::string outputApollon;
    public:

        CApollonnodeEntry(std::string alias, std::string ip, std::string privKey, std::string txHash, std::string outputApollon) {
            this->alias = alias;
            this->ip = ip;
            this->privKey = privKey;
            this->txHash = txHash;
            this->outputApollon = outputApollon;
        }

        const std::string& getAlias() const {
            return alias;
        }

        void setAlias(const std::string& alias) {
            this->alias = alias;
        }

        const std::string& getOutputApollon() const {
            return outputApollon;
        }

        void setOutputApollon(const std::string& outputApollon) {
            this->outputApollon = outputApollon;
        }

        const std::string& getPrivKey() const {
            return privKey;
        }

        void setPrivKey(const std::string& privKey) {
            this->privKey = privKey;
        }

        const std::string& getTxHash() const {
            return txHash;
        }

        void setTxHash(const std::string& txHash) {
            this->txHash = txHash;
        }

        const std::string& getIp() const {
            return ip;
        }

        void setIp(const std::string& ip) {
            this->ip = ip;
        }

        UniValue ToJSON(){
            UniValue ret(UniValue::VOBJ);
            UniValue outpoint(UniValue::VOBJ);
            UniValue authorityObj(UniValue::VOBJ);

            std::string authority = getIp();
            std::string ip   = authority.substr(0, authority.find(":"));
            std::string port = authority.substr(authority.find(":")+1, authority.length());

            outpoint.push_back(Pair("txid", getTxHash().substr(0,64)));
            outpoint.push_back(Pair("apollon", getOutputApollon()));
            authorityObj.push_back(Pair("ip", ip));
            authorityObj.push_back(Pair("port", port));

            ret.push_back(Pair("label", getAlias()));
            ret.push_back(Pair("isMine", true));
            ret.push_back(Pair("outpoint", outpoint));
            ret.push_back(Pair("authority", authorityObj));

            return ret;
        }
    };

    CApollonnodeConfig() {
        entries = std::vector<CApollonnodeEntry>();
    }

    void clear();
    bool read(std::string& strErr);
    void add(std::string alias, std::string ip, std::string privKey, std::string txHash, std::string outputApollon);

    std::vector<CApollonnodeEntry>& getEntries() {
        return entries;
    }

    int getCount() {
        return (int)entries.size();
    }

private:
    std::vector<CApollonnodeEntry> entries;


};


#endif /* SRC_APOLLONNODECONFIG_H_ */
