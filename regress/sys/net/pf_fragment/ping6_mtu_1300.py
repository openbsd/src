#!/usr/local/bin/python2.7
# check wether path mtu to dst is 1300

import os
import threading
from addr import *
from scapy.all import *

# work around the broken sniffing of packages with bad checksum
#a=srp1(eth, iface=SRC_IF, timeout=2)
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
hdr=IPv6(src=SRC_OUT6, dst=dstaddr)/ICMPv6EchoRequest(id=pid)
payload="a" * (1400 - len(str(hdr)))
ip=hdr/payload
eth=Ether(src=SRC_MAC, dst=PF_MAC)/ip

sniffer = Sniff1();
sniffer.filter = "ip6 and dst %s and icmp6" % SRC_OUT6
sniffer.start()
time.sleep(1)
sendp(eth, iface=SRC_IF)
sniffer.join(timeout=5)
a = sniffer.packet

if a is None:
	print "no packet sniffed"
	exit(2)
if a and a.type == ETH_P_IPV6 and \
    ipv6nh[a.payload.nh] == 'ICMPv6' and \
    icmp6types[a.payload.payload.type] == 'Packet too big':
	mtu=a.payload.payload.mtu
	print "mtu=%d" % (mtu)
	if mtu == 1300:
		exit(0)
	print "MTU!=1300"
	exit(1)
print "MTU=UNKNOWN"
exit(2)
