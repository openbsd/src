#!/usr/local/bin/python2.7
# send a ping6 packet with routing header type 0
# try to source route
# hide the routing header behind a fragment header to avoid header scan
# we expect an ICMP6 error, as we do not support source routing

import os
from addr import *
from scapy.all import *

pid=os.getpid() & 0xffff
payload="ABCDEFGHIJKLMNOP"
packet=IPv6(src=SRC_OUT6, dst=DST_IN6)/\
    IPv6ExtHdrFragment(id=pid)/\
    IPv6ExtHdrRouting(addresses=[SRT_IN6, SRT_OUT6], segleft=2)/\
    ICMPv6EchoRequest(id=pid, data=payload)
eth=Ether(src=SRC_MAC, dst=DST_MAC)/packet

if os.fork() == 0:
	time.sleep(1)
	sendp(eth, iface=SRC_IF)
	os._exit(0)

ans=sniff(iface=SRC_IF, timeout=3, filter=
    "ip6 and dst "+SRC_OUT6+" and icmp6")
for a in ans:
	if a and a.type == ETH_P_IPV6 and \
	    ipv6nh[a.payload.nh] == 'ICMPv6' and \
	    icmp6types[a.payload.payload.type] == 'Parameter problem':
		pprob=a.payload.payload
		code=pprob.code
		print "code=%#d" % (code)
		if code != 0:
			print "WRONG PARAMETER PROBLEM CODE"
			exit(2)
		ptr=pprob.ptr
		print "ptr=%#d" % (ptr)
		if ptr != 50:
			print "WRONG PARAMETER PROBLEM POINTER"
			exit(2)
		exit(0)
print "NO ICMP6 PARAMETER PROBLEM"
exit(1)
