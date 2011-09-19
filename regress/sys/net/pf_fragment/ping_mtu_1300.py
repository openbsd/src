#!/usr/local/bin/python2.7
# check wether path mtu to dst is 1300

import os
from addr import * 
from scapy.all import *

dstaddr=sys.argv[1]
pid=os.getpid()
payload="a" * 1452
a=srp1(Ether(src=SRC_MAC, dst=PF_MAC)/IP(flags="DF", src=SRC_OUT, dst=dstaddr)/
    ICMP(id=pid)/payload, iface=SRC_IF, timeout=2)
if a and a.payload.payload.type==3 and a.payload.payload.code==4:
	mtu=a.payload.payload.unused
	print "mtu=%d" % (mtu)
	if mtu == 1300:
		exit(0)
	print "MTU!=1300"
	exit(1)
print "MTU=UNKNOWN"
exit(2)
