#!/usr/local/bin/python2.7
# send a ping6 packet with routing header type 0
# the address list is empty
# hide the routing header behind a fragment header to avoid header scan
# we expect an echo reply, as there are no more hops

import os
from addr import *
from scapy.all import *

pid=os.getpid() & 0xffff
payload="ABCDEFGHIJKLMNOP"
packet=IPv6(src=SRC_OUT6, dst=DST_IN6)/\
    IPv6ExtHdrFragment(id=pid)/\
    IPv6ExtHdrRouting(addresses=[])/\
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
	    icmp6types[a.payload.payload.type] == 'Echo Reply':
		reply=a.payload.payload
		id=reply.id
		print "id=%#x" % (id)
		if id != pid:
			print "WRONG ECHO REPLY ID"
			exit(2)
		data=reply.data
		print "payload=%s" % (data)
		if data != payload:
			print "WRONG PAYLOAD"
			exit(2)
		exit(0)
print "NO ICMP6 ECHO REPLY"
exit(1)
