#!/usr/local/bin/python2.7
# check wether path mtu to dst is as expected

import os
from addr import *
from scapy.all import *

dstaddr=sys.argv[1]
expect=int(sys.argv[2])
pid=os.getpid()
payload="a" * 1452
ip=IP(flags="DF", src=SRC_OUT, dst=dstaddr)/ICMP(id=pid)/payload
iplen=IP(str(ip)).len
eth=Ether(src=SRC_MAC, dst=PF_MAC)/ip
a=srp1(eth, iface=SRC_IF, timeout=2)
if a and a.payload.payload.type==3 and a.payload.payload.code==4:
	mtu=a.payload.payload.unused
	print "mtu=%d" % (mtu)
	if mtu != expect:
		print "MTU!=%d" % (expect)
		exit(1)
	len=a.payload.payload.payload.len
	if len != iplen:
		print "IP len %d!=%d" % (len, iplen)
		exit(1)
	exit(0)
print "MTU=UNKNOWN"
exit(2)
