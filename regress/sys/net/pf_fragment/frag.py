#!/usr/local/bin/python2.7
# send 2 non-overlapping ping fragments

import os
from addr import *
from scapy.all import *

dstaddr=sys.argv[1]
pid=os.getpid() & 0xffff
payload="ABCDEFGHIJKLOMNO"
packet=IP(src=SRC_OUT, dst=dstaddr)/ICMP(id=pid)/payload
frag0=str(packet)[20:36]
frag1=str(packet)[36:44]
pkt0=IP(src=SRC_OUT, dst=dstaddr, proto=1, id=pid, frag=0, flags='MF')/frag0
pkt1=IP(src=SRC_OUT, dst=dstaddr, proto=1, id=pid, frag=2)/frag1
eth=[]
eth.append(Ether(src=SRC_MAC, dst=PF_MAC)/pkt0)
eth.append(Ether(src=SRC_MAC, dst=PF_MAC)/pkt1)

if os.fork() == 0:
	time.sleep(1)
	sendp(eth, iface=SRC_IF)
	os._exit(0)

ans=sniff(iface=SRC_IF, timeout=3, filter=
    "ip and src "+dstaddr+" and dst "+SRC_OUT+" and icmp")
a=ans[0]
if a and a.type == ETH_P_IP and \
    a.payload.proto == 1 and \
    a.payload.frag == 0 and a.payload.flags == 0 and \
    icmptypes[a.payload.payload.type] == 'echo-reply':
	id=a.payload.payload.id
	print "id=%#x" % (id)
	if id != pid:
		print "WRONG ECHO REPLY ID"
		exit(2)
	load=a.payload.payload.payload.load
	print "payload=%s" % (load)
	if load == payload:
		exit(0)
	print "PAYLOAD!=%s" % (payload)
	exit(1)
print "NO ECHO REPLY"
exit(2)
