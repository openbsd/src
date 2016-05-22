#!/usr/local/bin/python2.7
# end of new fragment overlaps old one

#     |XXXXX----|
# |--------|

# RFC 5722 drop overlapping fragments

import os
from addr import *
from scapy.all import *

dstaddr=sys.argv[1]
pid=os.getpid() & 0xffff
payload="ABCDEFGHIJKLOMNO"
dummy="01234567"
packet=IPv6(src=SRC_OUT6, dst=dstaddr)/ICMPv6EchoRequest(id=pid, data=payload)
frag0=IPv6ExtHdrFragment(nh=58, id=pid, m=1)/str(packet)[40:56]
frag1=IPv6ExtHdrFragment(nh=58, id=pid, offset=1)/(dummy+str(packet)[56:64])
pkt0=IPv6(src=SRC_OUT6, dst=dstaddr)/frag0
pkt1=IPv6(src=SRC_OUT6, dst=dstaddr)/frag1
eth=[]
eth.append(Ether(src=SRC_MAC, dst=PF_MAC)/pkt1)
eth.append(Ether(src=SRC_MAC, dst=PF_MAC)/pkt0)

if os.fork() == 0:
	time.sleep(1)
	sendp(eth, iface=SRC_IF)
	os._exit(0)

ans=sniff(iface=SRC_IF, timeout=3, filter=
    "ip6 and src "+dstaddr+" and dst "+SRC_OUT6+" and icmp6")
if len(ans) == 0:
	print "no reply"
	exit(0)
a=ans[0]
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
print "NO ECHO REPLY"
exit(2)
