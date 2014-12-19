#!/usr/local/bin/python2.7
# send 3 non-overlapping ping6 fragments in all possible orders

# |----|
#      |----|
#           |----|

import os
from addr import *
from scapy.all import *

permute=[]
permute.append([0,1,2])
permute.append([0,2,1])
permute.append([1,0,2])
permute.append([2,0,1])
permute.append([1,2,0])
permute.append([2,1,0])

pid=os.getpid()
payload="ABCDEFGHIJKLMNOP"
for p in permute:
	pid += 1
	packet=IPv6(src=SRC_OUT6, dst=DST_IN6)/ \
	    ICMPv6EchoRequest(id=pid, data=payload)
	frag=[]
	frag.append(IPv6ExtHdrFragment(nh=58, id=pid, m=1)/ \
	    str(packet)[40:48])
	frag.append(IPv6ExtHdrFragment(nh=58, id=pid, offset=1, m=1)/ \
	    str(packet)[48:56])
	frag.append(IPv6ExtHdrFragment(nh=58, id=pid, offset=2)/ \
	    str(packet)[56:64])
	eth=[]
	for i in range(3):
		pkt=IPv6(src=SRC_OUT6, dst=DST_IN6)/frag[p[i]]
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
				break
			print "PAYLOAD!=%s" % (payload)
			exit(1)
	else:
		print "NO ECHO REPLY"
		exit(2)
