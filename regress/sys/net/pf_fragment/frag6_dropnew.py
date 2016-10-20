#!/usr/local/bin/python2.7
# old fragment completely overlaps new one

#      |------------|
#          |XXXX|
# |----|

# RFC 5722 drop overlapping fragments

import os
import threading
from addr import *
from scapy.all import *

class Sniff1(threading.Thread):
	filter = None
	captured = None
	packet = None
	def run(self):
		self.captured = sniff(iface=SRC_IF, filter=self.filter,
		    count=1, timeout=3)
		if self.captured:
			self.packet = self.captured[0]

dstaddr=sys.argv[1]
pid=os.getpid() & 0xffff
payload="ABCDEFGHIJKLOMNOQRSTUVWX"
dummy="01234567"
packet=IPv6(src=SRC_OUT6, dst=dstaddr)/ICMPv6EchoRequest(id=pid, data=payload)
frag0=IPv6ExtHdrFragment(nh=58, id=pid, offset=0, m=1)/str(packet)[40:48]
frag1=IPv6ExtHdrFragment(nh=58, id=pid, offset=2, m=1)/dummy
frag2=IPv6ExtHdrFragment(nh=58, id=pid, offset=1)/str(packet)[48:72]
pkt0=IPv6(src=SRC_OUT6, dst=dstaddr)/frag0
pkt1=IPv6(src=SRC_OUT6, dst=dstaddr)/frag1
pkt2=IPv6(src=SRC_OUT6, dst=dstaddr)/frag2
eth=[]
eth.append(Ether(src=SRC_MAC, dst=PF_MAC)/pkt2)
eth.append(Ether(src=SRC_MAC, dst=PF_MAC)/pkt1)
eth.append(Ether(src=SRC_MAC, dst=PF_MAC)/pkt0)

sniffer = Sniff1();
sniffer.filter = "ip6 and src %s and dst %s and icmp6" % (dstaddr, SRC_OUT6)
sniffer.start()
time.sleep(1)
sendp(eth, iface=SRC_IF)
sniffer.join(timeout=5)
a = sniffer.packet

if a is None:
	print "no reply"
	exit(0)
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
