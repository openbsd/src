#!/usr/local/bin/python2.7
# send fragments of a large packet that has to be refragmented by reflector

# |--------|
#          |------------------|
#                              ...
#                                 |------------------|
#                                                    |----|

import os
from addr import *
from scapy.all import *

pid=os.getpid() & 0xffff
payload=100 * "ABCDEFGHIJKLMNOP"
packet=IPv6(src=SRC_OUT6, dst=DST_IN6)/ICMPv6EchoRequest(id=pid, data=payload)
request_cksum=ICMPv6Unknown(str(packet.payload)).cksum
print "request cksum=%#x" % (request_cksum)
frag=[]
frag.append(IPv6ExtHdrFragment(nh=58, id=pid, m=1)/str(packet)[40:56])
offset=2
chunk=4
while 40+8*(offset+chunk) < len(payload):
	frag.append(IPv6ExtHdrFragment(nh=58, id=pid, offset=offset, m=1)/
	    str(packet)[40+(8*offset):40+8*(offset+chunk)])
	offset+=chunk
frag.append(IPv6ExtHdrFragment(nh=58, id=pid, offset=offset)/
    str(packet)[40+(8*offset):])
eth=[]
for f in frag:
	pkt=IPv6(src=SRC_OUT6, dst=DST_IN6)/f
	eth.append(Ether(src=SRC_MAC, dst=DST_MAC)/pkt)

if os.fork() == 0:
	time.sleep(1)
	sendp(eth, iface=SRC_IF)
	os._exit(0)

ans=sniff(iface=SRC_IF, timeout=3, filter=
    "ip6 and src "+DST_IN6+" and dst "+SRC_OUT6+" and proto ipv6-frag")
for a in ans:
	if a and a.type == ETH_P_IPV6 and \
	    ipv6nh[a.payload.nh] == 'Fragment Header' and \
	    a.payload.payload.offset == 0 and \
	    ipv6nh[a.payload.payload.nh] == 'ICMPv6' and \
	    icmp6types[a.payload.payload.payload.type] == 'Echo Reply':
		id=a.payload.payload.payload.id
		print "id=%#x" % (id)
		if id != pid:
			print "WRONG ECHO REPLY ID"
			exit(2)
		reply_cksum=a.payload.payload.payload.cksum
		print "reply cksum=%#x" % (reply_cksum)
		# change request checksum incrementaly and check with reply
		diff_cksum=~(~reply_cksum+~(~request_cksum+~0x8000+0x8100))
		if  diff_cksum != -1:
			print "CHECKSUM ERROR diff cksum=%#x" % (diff_cksum)
			exit(1)
		exit(0)
print "NO ECHO REPLY"
exit(2)
