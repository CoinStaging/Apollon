// Copyright (c) 2018 Tadhg Riordan Zcoin Developer
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activeapollonnode.h"
#include "validationinterface.h"
#include "apollonnodeman.h"
#include "univalue.h"
#include "apollonnode-sync.h"
#include "apollonnodeconfig.h"
#include "client-api/server.h"
#include "client-api/protocol.h"
#include <client-api/wallet.h>
#include <unordered_map>

using namespace std;

bool GetApollonnodePayeeAddress(const std::string& txHash, const std::string& n, CBitcoinAddress& address){

    const CWalletTx* wtx = pwalletMain->GetWalletTx(uint256S(txHash));
    if(wtx==NULL)
        return false;

    CTxDestination destination;
    const CTxOut &txout = wtx->vout[stoi(n)];
    if (!ExtractDestination(txout.scriptPubKey, destination))
        return false;

    address.Set(destination);

    return true;
}

UniValue apollonnodekey(Type type, const UniValue& data, const UniValue& auth, bool fHelp){

    switch(type){
        case Update: {
            UniValue key(UniValue::VOBJ);
            CKey secret;
            secret.MakeNewKey(false);

            key.push_back(Pair("key", CBitcoinSecret(secret).ToString()));

            return key;
            break;
        }
        default: {
            throw JSONAPIError(API_TYPE_NOT_IMPLEMENTED, "Error: type does not exist for method called, or no type passed where method requires it.");
        }
    }
    return true;
}

UniValue apollonnodecontrol(Type type, const UniValue& data, const UniValue& auth, bool fHelp){

    switch(type){
        case Update: {
            string method;
            try {
                method = find_value(data, "method").get_str();
            }catch (const std::exception& e){
                throw JSONAPIError(API_INVALID_PARAMETER, "Invalid, missing or duplicate parameter");
            }
            
            UniValue overall(UniValue::VOBJ);
            UniValue detail(UniValue::VOBJ);
            UniValue ret(UniValue::VOBJ);
            
            int nSuccessful = 0;
            int nFailed = 0;

            if (method == "start-alias") {

                string alias;
                try {
                    alias = find_value(data, "alias").get_str();
                }catch (const std::exception& e){
                    throw JSONAPIError(API_INVALID_PARAMETER, "Invalid, missing or duplicate parameter");
                }

                bool fFound = false;

                UniValue status(UniValue::VOBJ);
                status.push_back(Pair("alias", alias));

                BOOST_FOREACH(CApollonnodeConfig::CApollonnodeEntry mne, apollonnodeConfig.getEntries()) {
                    if (mne.getAlias() == alias) {
                        fFound = true;
                        std::string strError;
                        CApollonnodeBroadcast mnb;

                        bool fResult = CApollonnodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(),
                                                                    mne.getOutputApollon(), strError, mnb);
                        status.push_back(Pair("success", fResult));
                        if (fResult) {
                            nSuccessful++;
                            mnodeman.UpdateApollonnodeList(mnb);
                            mnb.RelayApollonNode();
                        } else {
                            nFailed++;
                            status.push_back(Pair("info", strError));
                        }
                        mnodeman.NotifyApollonnodeUpdates();
                        break;
                    }
                }

                if (!fFound) {
                    nFailed++;
                    status.push_back(Pair("success", false));
                    status.push_back(Pair("info", "Could not find alias in config. Verify with list-conf."));
                }

                detail.push_back(Pair("status", status));
            }

            else if (method == "start-all" || method == "start-missing") {
                {
                    LOCK(pwalletMain->cs_wallet);
                    EnsureWalletIsUnlocked();
                }

                if ((method == "start-missing") && !apollonnodeSync.IsApollonnodeListSynced()) {
                    throw JSONAPIError(API_CLIENT_IN_INITIAL_DOWNLOAD,
                                       "You can't use this command until apollonnode list is synced");
                }

                BOOST_FOREACH(CApollonnodeConfig::CApollonnodeEntry mne, apollonnodeConfig.getEntries()) {
                    std::string strError;

                    CTxIn vin = CTxIn(uint256S(mne.getTxHash()), uint32_t(atoi(mne.getOutputApollon().c_str())));
                    CApollonnode *pmn = mnodeman.Find(vin);
                    CApollonnodeBroadcast mnb;

                    if (method == "start-missing" && pmn) continue;

                    bool fResult = CApollonnodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(),
                                                                mne.getOutputApollon(), strError, mnb);

                    UniValue status(UniValue::VOBJ);
                    status.push_back(Pair("alias", mne.getAlias()));
                    status.push_back(Pair("success", fResult));

                    if (fResult) {
                        nSuccessful++;
                        mnodeman.UpdateApollonnodeList(mnb);
                        mnb.RelayApollonNode();
                    } else {
                        nFailed++;
                        status.push_back(Pair("info", strError));
                    }

                    detail.push_back(Pair("status", status));
                }
                mnodeman.NotifyApollonnodeUpdates();

            }

            else if(method=="update-status"){
                
            }
            else {
                throw runtime_error("Method not found.");
            }

            overall.push_back(Pair("successful", nSuccessful));
            overall.push_back(Pair("failed", nFailed));
            overall.push_back(Pair("total", nSuccessful + nFailed));

            ret.push_back(Pair("overall", overall));
            ret.push_back(Pair("detail", detail));

            return ret;
            break;
        }
        default: {
            throw JSONAPIError(API_TYPE_NOT_IMPLEMENTED, "Error: type does not exist for method called, or no type passed where method requires it.");
        }
    }
    return true;
}

UniValue apollonnodelist(Type type, const UniValue& data, const UniValue& auth, bool fHelp){

    switch(type){
        case Initial: {
            UniValue data(UniValue::VOBJ);
            UniValue nodes(UniValue::VOBJ);

            int fApollon = 0;
            BOOST_FOREACH(CApollonnodeConfig::CApollonnodeEntry mne, apollonnodeConfig.getEntries()) {
                const std::string& txHash = mne.getTxHash();
                const std::string& outputApollon = mne.getOutputApollon();
                CBitcoinAddress address;
                std::string key = txHash + outputApollon;
                CApollonnode* mn = mnodeman.Find(txHash, outputApollon);

                UniValue node(UniValue::VOBJ);
                if(mn==NULL){
                    node = mne.ToJSON();
                    node.push_back(Pair("position", fApollon++));
                    if(GetApollonnodePayeeAddress(txHash, outputApollon, address))
                        node.push_back(Pair("payeeAddress", address.ToString()));
                }else{
                    node = mn->ToJSON();
                }
                nodes.replace(key, node);
            }

            /*
             * If the Apollonnode list is not yet synced, return the wallet Apollonnodes, as described in apollonnode.conf
             * if it is, process all Apollonnodes, and return along with wallet Apollonnodes.
             * (if the wallet Apollonnode has started, it will be replaced in the synced list).
             */
            if(!apollonnodeSync.IsSynced()){
                data.push_back(Pair("nodes", nodes));
                data.push_back(Pair("total", mnodeman.CountApollonnodes()));
                return data;
            }

            std::vector <CApollonnode> vApollonnodes = mnodeman.GetFullApollonnodeVector();
            BOOST_FOREACH(CApollonnode & mn, vApollonnodes) {
                std::string txHash = mn.vin.prevout.hash.ToString().substr(0,64);
                std::string outputApollon = to_string(mn.vin.prevout.n);
                std::string key = txHash + outputApollon;

                // only process wallet Apollonnodes - they are already in "nodes", so if we find it, replace with update
                if(!find_value(nodes, key).isNull())
                    nodes.replace(key, mn.ToJSON());
            }

            data.push_back(Pair("nodes", nodes));
            data.push_back(Pair("total", mnodeman.CountApollonnodes()));
            return data;
            break;
        }
        default: {
            throw JSONAPIError(API_TYPE_NOT_IMPLEMENTED, "Error: type does not exist for method called, or no type passed where method requires it.");
        }
    }

    return true;
}

UniValue apollonnodeupdate(Type type, const UniValue& data, const UniValue& auth, bool fHelp){
    UniValue ret(UniValue::VOBJ);
    UniValue outpoint(UniValue::VOBJ);
    string key;
    // We already have the return data in the "data" object, here we simply form the key.
    try {
        outpoint = find_value(data, "outpoint").get_obj();
        key = find_value(outpoint, "txid").get_str() +  find_value(outpoint, "apollon").get_str();
    }catch (const std::exception& e){
        throw JSONAPIError(API_INVALID_PARAMETER, "Invalid, missing or duplicate parameter");
    }
    ret.push_back(Pair(key, data));
    return ret;
}

static const CAPICommand commands[] =
{ //  category              collection         actor (function)          authPort   authPassphrase   warmupOk
  //  --------------------- ------------       ----------------          -------- --------------   --------
    { "apollonnode",              "apollonnodeControl",    &apollonnodecontrol,            true,      true,            false  },
    { "apollonnode",              "apollonnodeKey",        &apollonnodekey,                true,      false,           false  },
    { "apollonnode",              "apollonnodeList",       &apollonnodelist,               true,      false,           false  },
    { "apollonnode",              "apollonnodeUpdate",     &apollonnodeupdate,             true,      false,           false  }
};
void RegisterApollonnodeAPICommands(CAPITable &tableAPI)
{
    for (unsigned int vcxap = 0; vcxap < ARRAYLEN(commands); vcxap++)
        tableAPI.appendCommand(commands[vcxap].collection, &commands[vcxap]);
}
