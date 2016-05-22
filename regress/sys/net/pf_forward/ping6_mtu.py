#!/usr/local/bin/python2.7
# check wether path mtu to dst is as expected

import os
from addr import *
from scapy.all import *

# usage: ping6_mtu src dst size icmp6-size

srcaddr=sys.argv[1]
dstaddr=sys.argv[2]
size=int(sys.argv[3])
expect=int(sys.argv[4])
pid=os.getpid() & 0xffff
hdr=IPv6(src=srcaddr, dst=dstaddr)/ICMPv6EchoRequest(id=pid)
payload="a" * (size - len(str(hdr)))
ip=hdr/payload
iplen=IPv6(str(ip)).plen
eth=Ether(src=SRC_MAC, dst=PF_MAC)/ip

# work around the broken sniffing of packages with bad checksum
#a=srp1(eth, iface=SRC_IF, timeout=2)
if os.fork() == 0:
	time.sleep(1)
	sendp(eth, iface=SRC_IF)
	os._exit(0)
ans=sniff(iface=SRC_IF, timeout=3, filter=
    "ip6 and dst "+srcaddr+" and icmp6")
if len(ans) == 0:
	print "no packet sniffed"
	exit(2)
a=ans[0]

if a and a.type == ETH_P_IPV6 and \
    ipv6nh[a.payload.nh] == 'ICMPv6' and \
    icmp6types[a.payload.payload.type] == 'Packet too big':
	mtu=a.payload.payload.mtu
	print "mtu=%d" % (mtu)
	if mtu != expect:
		print "MTU!=%d" % (expect)
		exit(1)
	iip=a.payload.payload.payload
	iiplen=iip.plen
	if iiplen != iplen:
		print "inner IPv6 plen %d!=%d" % (iiplen, iplen)
		exit(1)
	isrc=iip.src
	if isrc != srcaddr:
		print "inner IPv6 src %d!=%d" % (isrc, srcaddr)
		exit(1)
	idst=iip.dst
	if idst != dstaddr:
		print "inner IPv6 dst %d!=%d" % (idst, dstaddr)
		exit(1)
	exit(0)
print "MTU=UNKNOWN"
exit(2)
