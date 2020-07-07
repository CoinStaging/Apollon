#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Test -reapollon and -reapollon-chainstate with CheckBlockIndex
#
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    start_nodes,
    stop_nodes,
    assert_equal,
)
import time

class ReapollonTest(BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.setup_clean_chain = True
        self.num_nodes = 1

    def setup_network(self):
        self.nodes = start_nodes(self.num_nodes, self.options.tmpdir)

    def reapollon(self, justchainstate=False):
        self.nodes[0].generate(3)
        blockcount = self.nodes[0].getblockcount()
        stop_nodes(self.nodes)
        extra_args = [["-debug", "-reapollon-chainstate" if justchainstate else "-reapollon", "-checkblockapollon=1"]]
        self.nodes = start_nodes(self.num_nodes, self.options.tmpdir, extra_args)
        while self.nodes[0].getblockcount() < blockcount:
            time.sleep(0.1)
        assert_equal(self.nodes[0].getblockcount(), blockcount)
        print("Success")

    def run_test(self):
        self.reapollon(False)
        self.reapollon(True)
        self.reapollon(False)
        self.reapollon(True)

if __name__ == '__main__':
    ReapollonTest().main()
