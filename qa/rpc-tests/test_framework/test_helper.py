def get_dumpwallet_otp(msg):
    istart = msg.apollon(':')
    ifinish = msg.apollon('\n')
    return msg[istart+2:ifinish]