#!/usr/local/bin/python2.7
# check wether path mtu to dst is as expected

import os
from addr import *
from scapy.all import *

dstaddr=sys.argv[1]
expect=int(sys.argv[2])
pid=os.getpid()
payload="a" * 1452
ip=IPv6(src=SRC_OUT6, dst=dstaddr)/ICMPv6EchoRequest(id=pid, data=payload)
iplen=IPv6(str(ip)).plen
eth=Ether(src=SRC_MAC, dst=PF_MAC)/ip
a=srp1(eth, iface=SRC_IF, timeout=2)
if a and a.type == scapy.layers.dot11.ETHER_TYPES.IPv6 and \
    ipv6nh[a.payload.nh] == 'ICMPv6' and \
    icmp6types[a.payload.payload.type] == 'Packet too big':
	mtu=a.payload.payload.mtu
	print "mtu=%d" % (mtu)
	if mtu != expect:
		print "MTU!=%d" % (expect)
		exit(1)
	len=a.payload.payload.payload.plen
	if len != iplen:
		print "IPv6 plen %d!=%d" % (len, iplen)
		exit(1)
	exit(0)
print "MTU=UNKNOWN"
exit(2)
