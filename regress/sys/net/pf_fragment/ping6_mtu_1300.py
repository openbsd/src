#!/usr/local/bin/python2.7
# check wether path mtu to dst is 1300

import os
from addr import *
from scapy.all import *

dstaddr=sys.argv[1]
pid=os.getpid()
payload="a" * 1452
a=srp1(Ether(src=SRC_MAC, dst=PF_MAC)/IPv6(src=SRC_OUT6, dst=dstaddr)/
    ICMPv6EchoRequest(id=pid, data=payload), iface=SRC_IF, timeout=2)
if a and a.type == ETH_P_IPV6 and \
    ipv6nh[a.payload.nh] == 'ICMPv6' and \
    icmp6types[a.payload.payload.type] == 'Packet too big':
	mtu=a.payload.payload.mtu
	print "mtu=%d" % (mtu)
	if mtu == 1300:
		exit(0)
	print "MTU!=1300"
	exit(1)
print "MTU=UNKNOWN"
exit(2)
