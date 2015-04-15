#!/usr/local/bin/python2.7
# send 2 non-overlapping ping6 fragments with ethernet padding

# |--------|XX|
#          |----|XX|

import os
from addr import *
from scapy.all import *

pid=os.getpid()
payload="ABCDEFGHIJKLMNOP"
padding="0123"
packet=IPv6(src=SRC_OUT6, dst=DST_IN6)/ICMPv6EchoRequest(id=pid, data=payload)
frag=[]
frag.append(IPv6ExtHdrFragment(nh=58, id=pid, m=1)/str(packet)[40:56])
frag.append(IPv6ExtHdrFragment(nh=58, id=pid, offset=2)/str(packet)[56:64])
eth=[]
for f in frag:
	pkt=str(IPv6(src=SRC_OUT6, dst=DST_IN6)/f) + padding
	eth.append(Ether(src=SRC_MAC, dst=DST_MAC, type=0x86dd)/pkt)

if os.fork() == 0:
	time.sleep(1)
	sendp(eth, iface=SRC_IF)
	os._exit(0)

ans=sniff(iface=SRC_IF, timeout=3, filter=
    "ip6 and src "+DST_IN6+" and dst "+SRC_OUT6+" and icmp6")
for a in ans:
	if a and a.type == ETH_P_IPV6 and \
	    ipv6nh[a.payload.nh] == 'ICMPv6' and \
	    icmp6types[a.payload.payload.type] == 'Echo Reply':
		id=a.payload.payload.id
		print "id=%#x" % (id)
		if id != pid:
			print "WRONG ECHO REPLY ID"
			exit(2)
		data=a.payload.payload.data
		print "payload=%s" % (data)
		if data == payload:
			exit(0)
		print "PAYLOAD!=%s" % (payload)
		exit(1)
print "NO ECHO REPLY"
exit(2)
