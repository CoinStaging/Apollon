// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ACTIVEAPOLLONNODE_H
#define ACTIVEAPOLLONNODE_H

#include "net.h"
#include "key.h"
#include "wallet/wallet.h"

class CActiveApollonnode;

static const int ACTIVE_APOLLONNODE_INITIAL          = 0; // initial state
static const int ACTIVE_APOLLONNODE_SYNC_IN_PROCESS  = 1;
static const int ACTIVE_APOLLONNODE_INPUT_TOO_NEW    = 2;
static const int ACTIVE_APOLLONNODE_NOT_CAPABLE      = 3;
static const int ACTIVE_APOLLONNODE_STARTED          = 4;

extern CActiveApollonnode activeApollonnode;

// Responsible for activating the Apollonnode and pinging the network
class CActiveApollonnode
{
public:
    enum apollonnode_type_enum_t {
        APOLLONNODE_UNKNOWN = 0,
        APOLLONNODE_REMOTE  = 1,
        APOLLONNODE_LOCAL   = 2
    };

private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

    apollonnode_type_enum_t eType;

    bool fPingerEnabled;

    /// Ping Apollonnode
    bool SendApollonnodePing();

public:
    // Keys for the active Apollonnode
    CPubKey pubKeyApollonnode;
    CKey keyApollonnode;

    // Initialized while registering Apollonnode
    CTxIn vin;
    CService service;

    int nState; // should be one of ACTIVE_APOLLONNODE_XXXX
    std::string strNotCapableReason;

    CActiveApollonnode()
        : eType(APOLLONNODE_UNKNOWN),
          fPingerEnabled(false),
          pubKeyApollonnode(),
          keyApollonnode(),
          vin(),
          service(),
          nState(ACTIVE_APOLLONNODE_INITIAL)
    {}

    /// Manage state of active Apollonnode
    void ManageState();

    // Change state if different and publish update
    void ChangeState(int newState);

    std::string GetStateString() const;
    std::string GetStatus() const;
    std::string GetTypeString() const;

private:
    void ManageStateInitial();
    void ManageStateRemote();
    void ManageStateLocal();
};

#endif
