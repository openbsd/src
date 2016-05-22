#!/usr/local/bin/python2.7
# send 6 non-overlapping ping6 fragments in 75 seconds, timeout is 60

# |----|
#      |----|
#           |----|
#                |----|
#                     |----|      <--- timeout
#                          |----|

import os
from addr import *
from scapy.all import *

pid=os.getpid() & 0xffff
payload="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcd"
packet=IPv6(src=SRC_OUT6, dst=DST_IN6)/ICMPv6EchoRequest(id=pid, data=payload)
frag=[]
frag.append(IPv6ExtHdrFragment(nh=58, id=pid, m=1)/str(packet)[40:48])
frag.append(IPv6ExtHdrFragment(nh=58, id=pid, offset=1, m=1)/str(packet)[48:56])
frag.append(IPv6ExtHdrFragment(nh=58, id=pid, offset=2, m=1)/str(packet)[56:64])
frag.append(IPv6ExtHdrFragment(nh=58, id=pid, offset=3, m=1)/str(packet)[64:72])
frag.append(IPv6ExtHdrFragment(nh=58, id=pid, offset=4, m=1)/str(packet)[72:80])
frag.append(IPv6ExtHdrFragment(nh=58, id=pid, offset=5)/str(packet)[80:88])
eth=[]
for f in frag:
	pkt=IPv6(src=SRC_OUT6, dst=DST_IN6)/f
	eth.append(Ether(src=SRC_MAC, dst=DST_MAC)/pkt)

if os.fork() == 0:
	time.sleep(1)
	for e in eth:
		sendp(e, iface=SRC_IF)
		time.sleep(15)
	os._exit(0)

ans=sniff(iface=SRC_IF, timeout=90, filter=
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
			print "ECHO REPLY"
			exit(1)
		print "PAYLOAD!=%s" % (payload)
		exit(2)
print "no echo reply"
exit(0)
