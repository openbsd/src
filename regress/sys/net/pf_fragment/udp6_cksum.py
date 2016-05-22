#!/usr/local/bin/python2.7
# check udp6 checksum in returned icmp packet

import os
from addr import *
from scapy.all import *

dstaddr=sys.argv[1]
pid=os.getpid() & 0xffff
payload="a" * 1452
p=(Ether(src=SRC_MAC, dst=PF_MAC)/IPv6(src=SRC_OUT6, dst=dstaddr)/
    UDP(sport=pid,dport=9)/payload)
udpcksum=IPv6(str(p.payload)).payload.chksum
print "udpcksum=%#04x" % (udpcksum)
a=srp1(p, iface=SRC_IF, timeout=2)
if a and a.type == ETH_P_IPV6 and \
    ipv6nh[a.payload.nh] == 'ICMPv6' and \
    icmp6types[a.payload.payload.type] == 'Packet too big':
	outercksum=a.payload.payload.cksum
	print "outercksum=%#04x" % (outercksum)
	q=a.payload.payload.payload
	if ipv6nh[q.nh] == 'UDP':
		innercksum=q.payload.chksum
		print "innercksum=%#04x" % (innercksum)
		if innercksum == udpcksum:
			exit(0)
		print "INNERCKSUM!=UDPCKSUM"
		exit(1)
	print "NO INNER UDP PACKET"
	exit(2)
print "NO PACKET TOO BIG"
exit(2)
