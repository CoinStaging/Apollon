#!/usr/bin/env python3
import time
from decimal import *

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

from pprint import pprint

# Zapwallettxes, rescan, reapollon, reapollon-chainstate are not affect existing transactions


#1. Generate some blocks
#2. Mint apollons
#3. 2 Spend apollons in different time
#4. Send apollons
#5. Gerate blocks
#6. Remint some apollons
#7. Mint sigma coins
#8. 2 Spend in different time
#9. Send
#10. Restart with zapwallettxes=1
#11. Check all transactions shown properly as before restart 
#12. Restart with zapwallettxes=2
#13. Check all transactions shown properly as before restart 
#14. Restart with rescan
#15. Check all transactions shown properly as before restart 
#16. Restart with reapollon
#17. Check all transactions shown properly as before restart 
#18. Restart with reapollon-chainstate
#19. Check all transactions shown properly as before restart 

class TransactionsVerAfterRestartTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 4
        self.setup_clean_chain = False

    def setup_nodes(self):
        # This test requires mocktime
        enable_mocktime()
        return start_nodes(self.num_nodes, self.options.tmpdir)

    def run_test(self):
        getcontext().prec = 6

        #1. Generate some blocks
        self.nodes[0].generate(101)
        self.sync_all()

        apollon_denoms = [1, 10, 25, 50, 100]

        #2. Mint apollons
        for denom in apollon_denoms:
            self.nodes[0].mintzerocoin(denom)
            self.nodes[0].mintzerocoin(denom)

        #3. 2 Spend apollons
        self.nodes[0].generate(10)
        self.nodes[0].spendzerocoin(1)
        self.nodes[0].spendzerocoin(10)

        #4. Send apollons
        self.nodes[0].sendtoaddress('TNZMs3dtwRddC5BuZ9zQUdvksPUjmJPRfL', 25)

        #5. Gerate blocks
        self.nodes[0].generate(290)

        #6. Remint some apollons
        self.nodes[0].remintzerocointosigma(50)

        self.nodes[0].generate(10)

        sigma_denoms = [0.05, 0.1, 0.5, 1, 10, 25, 100]

        #7. Mint sigma coins
        for denom in sigma_denoms:
                 self.nodes[0].mint(denom)
                 self.nodes[0].mint(denom)

        self.nodes[0].generate(100)

        #8. 2 Spend in different time
        args = {'THAYjKnnCsN5xspnEcb1Ztvw4mSPBuwxzU': 100}
        self.nodes[0].spendmany("", args)

        args = {'THAYjKnnCsN5xspnEcb1Ztvw4mSPBuwxzU': 25}
        self.nodes[0].spendmany("", args)

        #9. Send
        self.nodes[0].sendtoaddress('TNZMs3dtwRddC5BuZ9zQUdvksPUjmJPRfL', 10)

        self.nodes[0].generate(10)

        transactions_before = self.nodes[0].listtransactions()

        self.nodes[0].stop()
        bitcoind_processes[0].wait()
        
        #10. Restart with zapwallettxes=1
        self.nodes[0] = start_node(0,self.options.tmpdir, ["-zapwallettxes=1"])

        #list of transactions should be same as initial after restart with flag
        transactions_after_zapwallettxes1 = self.nodes[0].listtransactions()

        #11. Check all transactions shown properly as before restart 
        assert transactions_before == transactions_after_zapwallettxes1, \
            'List of transactions after restart with zapwallettxes=1 unexpectedly changed.'
        
        self.nodes[0].stop()
        bitcoind_processes[0].wait()
        
        #12. Restart with zapwallettxes=2
        self.nodes[0] = start_node(0,self.options.tmpdir, ["-zapwallettxes=2"])
        
        #list of transactions should be same as initial after restart with flag
        transactions_after_zapwallettxes2 = self.nodes[0].listtransactions()

        #13. Check all transactions shown properly as before restart 
        assert transactions_before == transactions_after_zapwallettxes2, \
            'List of transactions after restart with zapwallettxes=2 unexpectedly changed.'

        self.nodes[0].stop()
        bitcoind_processes[0].wait()

        #14. Restart with rescan
        self.nodes[0] = start_node(0,self.options.tmpdir, ["-rescan"])
        
        #15. Check all transactions shown properly as before restart 
        transactions_after_rescan = self.nodes[0].listtransactions()

        assert transactions_before == transactions_after_rescan, \
            'List of transactions after restart with rescan unexpectedly changed.'

        last_block_height = self.nodes[0].getinfo()["blocks"]
        transactions_before_reapollon = self.nodes[0].listtransactions("*", 1000)

        self.nodes[0].stop()
        bitcoind_processes[0].wait()

        #16. Restart with reapollon
        self.nodes[0] = start_node(0,self.options.tmpdir, ["-reapollon"])

        while self.nodes[0].getinfo()["blocks"] != last_block_height:
            time.sleep(1)

        #17. Check all transactions shown properly as before restart
        tx_before = sorted(transactions_before_reapollon, key=lambda k: k['txid'], reverse=True)
        tx_after_reapollon = sorted(self.nodes[0].listtransactions("*", 1000), key=lambda k: k['txid'], reverse=True)

        assert tx_before == tx_after_reapollon, \
            'List of transactions after restart with reapollon unexpectedly changed.'

        self.nodes[0].stop()
        bitcoind_processes[0].wait()

        #18. Restart with reapollon-chainstate
        self.nodes[0] = start_node(0,self.options.tmpdir, ["-reapollon-chainstate"])

        time.sleep(5)
        
        #19. Check all transactions shown properly as before restart
        tx_after_reapollon_chainstate = sorted(self.nodes[0].listtransactions("*", 1000), key=lambda k: k['txid'], reverse=True)

        assert tx_before == tx_after_reapollon_chainstate, \
            'List of transactions after restart with reapollon-chainstate unexpectedly changed.'



if __name__ == '__main__':
    TransactionsVerAfterRestartTest().main()
