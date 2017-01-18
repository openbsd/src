#!/usr/local/bin/python2.7

print "fragment with dest option and atomic fragment without protocol header"

# |-IP-|-Frag-|-ExtDest-|-ICMP6-|-pay|
# |-- atomic fragment --|
#                                    |load-|

import os
from addr import *
from scapy.all import *

pid=os.getpid()
eid=pid & 0xffff
payload="ABCDEFGHIJKLMNOP"
packet=IPv6(src=SRC_OUT6, dst=DST_IN6)/IPv6ExtHdrDestOpt()/ \
    ICMPv6EchoRequest(id=eid, data=payload)
frag=[]
fid=pid & 0xffffffff
frag.append(IPv6ExtHdrFragment(nh=60, id=fid, m=1)/str(packet)[40:64])
frag.append(IPv6ExtHdrFragment(nh=60, id=fid)/str(packet)[40:48])
frag.append(IPv6ExtHdrFragment(nh=60, id=fid, offset=3)/str(packet)[64:72])
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
		if id != eid:
			print "WRONG ECHO REPLY ID"
			exit(2)
		data=a.payload.payload.data
		print "payload=%s" % (data)
		if data == payload:
			exit(0)
		print "PAYLOAD!=%s" % (payload)
		exit(2)
print "NO ECHO REPLY"
exit(1)
