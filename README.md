Apollon
===============

[![latest-release](https://img.shields.io/github/release/IndexChain/Apollon)](https://github.com/IndexChain/Apollon/releases)
[![GitHub last-release](https://img.shields.io/github/release-date/IndexChain/Apollon)](https://github.com/IndexChain/Apollon/releases)
[![GitHub downloads](https://img.shields.io/github/downloads/IndexChain/Apollon/total)](https://github.com/IndexChain/Apollon/releases)
[![GitHub commits-since-last-version](https://img.shields.io/github/commits-since/IndexChain/Apollon/latest/master)](https://github.com/IndexChain/Apollon/graphs/commit-activity)
[![GitHub commits-per-month](https://img.shields.io/github/commit-activity/m/IndexChain/Apollon)](https://github.com/IndexChain/Apollon/graphs/code-frequency)
[![GitHub last-commit](https://img.shields.io/github/last-commit/IndexChain/Apollon)](https://github.com/IndexChain/Apollon/commits/master)
[![Total alerts](https://img.shields.io/lgtm/alerts/g/IndexChain/Apollon.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/IndexChain/Apollon/alerts/)
[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/IndexChain/Apollon.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/IndexChain/Apollon/context:cpp)

What is Apollon Chain?
--------------

[Apollon Chain](https://indexchain.org) is designed from the ground up with a singular focal point: Privacy.

Utilizing privacy protocols informed by industry experts to create cryptosystems specifically designed to facilitate anonymous transactions! It is powered by the Sigma privacy protocol, based on the academic paper One-Out-Of-Many-Proofs: Or How to Leak a Secret and Spend a Coin (Jens Groth and Markulf Kohlweiss). It replaces the RSA accumulators through Pedersen commitments and eliminates the need for a trusted setup.


Why Apollon?
--------------
APOLLON CHAIN is designed for user privacy, shielding transactions with anonymous designations while deploying industry leading encryption methods. Apollon Chain is a complete solution, providing users with a fully private, secure, fast and decentralized solution. Protect your assets and remove banks from the equation. Avoid paying large sums with truly private transactions!
You do not have to fear about the blockage of your financial capabilities based on the whims of some power-hungry managers. Apollon Chain will allow you to become your own bank. You can spend your money safely and privately without leaving a trail of documents marking every step in your life. 

Who are we?
-------------- 
We are a community dedicated to privacy. Comprised of consummate professionals with a passion for privacy, our goal is eschewing the age of bank control over personal financial situation and to establish the power of choice. APOLLON is the ultimate solution, providing financial freedom coupled with opportunity.

===================

If you are already familiar with Docker, then running Apollon with Docker might be the the easier method for you. To run Apollon using this method, first install [Docker](https://store.docker.com/search?type=edition&offering=community). After this you may
continue with the following instructions.

Please note that we currently don't support the GUI when running with Docker. Therefore, you can only use RPC (via HTTP or the `apollon-cli` utility) to interact with Apollon via this method.

Pull our latest official Docker image:

```sh
docker pull IndexChain/Indexd
```

Start Apollon daemon:

```sh
docker run --detach --name indexd IndexChain/Indexd
```

View current block count (this might take a while since the daemon needs to find other nodes and download blocks first):

```sh
docker exec indexd apollon-cli getblockcount
```

View connected nodes:

```sh
docker exec indexd apollon-cli getpeerinfo
```

Stop daemon:

```sh
docker stop indexd
```

Backup wallet:

```sh
docker cp indexd:/home/indexd/.apollon/wallet.dat .
```

Start daemon again:

```sh
docker start indexd
```

Linux Build Instructions and Notes
==================================

Dependencies
----------------------
1.  Update packages

        sudo apt-get update

2.  Install required packages

        sudo apt-get install build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils libboost-all-dev libzmq3-dev libminizip-dev

3.  Install Berkeley DB 4.8

        sudo apt-get install software-properties-common
        sudo add-apt-repository ppa:bitcoin/bitcoin
        sudo apt-get update
        sudo apt-get install libdb4.8-dev libdb4.8++-dev

4.  Install QT 5

        sudo apt-get install libminiupnpc-dev
        sudo apt-get install libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler libqrencode-dev

Build
----------------------
1.  Clone the source:

        git clone https://github.com/IndexChain/Apollon

2.  Build Apollon-core:

    Configure and build the headless Apollon binaries as well as the GUI (if Qt is found).

    You can disable the GUI build by passing `--without-gui` to configure.
        
        ./autogen.sh
        ./configure
        make

3.  It is recommended to build and run the unit tests:

        make check


macOS Build Instructions and Notes
=====================================
See (doc/build-macos.md) for instructions on building on macOS.



Windows (64/32 bit) Build Instructions and Notes
=====================================
See (doc/build-windows.md) for instructions on building on Windows 64/32 bit.
