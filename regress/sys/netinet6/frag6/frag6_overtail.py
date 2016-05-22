#!/usr/local/bin/python2.7
# send ping6 fragment that overlaps the last fragment with the tail

#          |----|
#      |XXXXXXXX|
# |--------|

import os
from addr import *
from scapy.all import *

pid=os.getpid() & 0xffff
payload="ABCDEFGHIJKLMNOP"
dummy="0123456701234567"
packet=IPv6(src=SRC_OUT6, dst=DST_IN6)/ICMPv6EchoRequest(id=pid, data=payload)
frag=[]
frag.append(IPv6ExtHdrFragment(nh=58, id=pid, offset=2)/str(packet)[56:64])
frag.append(IPv6ExtHdrFragment(nh=58, id=pid, offset=1)/dummy)
frag.append(IPv6ExtHdrFragment(nh=58, id=pid, m=1)/str(packet)[40:56])
eth=[]
for f in frag:
	pkt=IPv6(src=SRC_OUT6, dst=DST_IN6)/f
	eth.append(Ether(src=SRC_MAC, dst=DST_MAC)/pkt)

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
			print "ECHO REPLY"
			exit(1)
		print "PAYLOAD!=%s" % (payload)
		exit(2)
print "no echo reply"
exit(0)
