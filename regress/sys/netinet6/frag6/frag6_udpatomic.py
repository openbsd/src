#!/usr/local/bin/python2.7
# send atomic udp fragment

# |--------|
#          |----|

import os
from addr import *
from scapy.all import *

pid=os.getpid() & 0xffff
payload="ABCDEFGHIJKLMNOP"
packet=IPv6(src=SRC_OUT6, dst=DST_IN6)/UDP(sport=pid, dport=7)/payload
frag=[]
frag.append(IPv6ExtHdrFragment(nh=17, id=pid)/str(packet)[40:64])
eth=[]
for f in frag:
	pkt=IPv6(src=SRC_OUT6, dst=DST_IN6)/f
	eth.append(Ether(src=SRC_MAC, dst=DST_MAC)/pkt)

if os.fork() == 0:
	time.sleep(1)
	sendp(eth, iface=SRC_IF)
	os._exit(0)

ans=sniff(iface=SRC_IF, timeout=3, filter=
    "ip6 and src "+DST_IN6+" and dst "+SRC_OUT6+" and udp")
for a in ans:
	if a and a.type == ETH_P_IPV6 and \
	    ipv6nh[a.payload.nh] == 'UDP' and \
	    a.payload.payload.sport == 7:
		port=a.payload.payload.dport
		print "port=%d" % (port)
		if port != pid:
			print "WRONG UDP ECHO REPLY PORT"
			exit(2)
		data=a.payload.payload.load
		print "payload=%s" % (data)
		if data == payload:
			exit(0)
		print "PAYLOAD!=%s" % (payload)
		exit(1)
print "NO UDP ECHO REPLY"
exit(2)
