#!/usr/local/bin/python2.7
# send Neighbor Unreachability Detection neighbor solicitation
# expect an neighbor advertisement answer and check it

import os
from addr import *
from scapy.all import *

# link-local solicited-node multicast address
def nsma(a):
	n = inet_pton(socket.AF_INET6, a)
	return inet_ntop(socket.AF_INET6, in6_getnsma(n))

# ethernet multicast address of multicast address
def nsmac(a):
	n = inet_pton(socket.AF_INET6, a)
	return in6_getnsmac(n)

# ethernet multicast address of solicited-node multicast address
def nsmamac(a):
	return nsmac(nsma(a))

# link-local address
def lla(m):
	return "fe80::"+in6_mactoifaceid(m)

ip=IPv6(src=SRC_OUT6, dst=DST_IN6)/ICMPv6ND_NS(tgt=DST_IN6)
eth=Ether(src=SRC_MAC, dst=DST_MAC)/ip

if os.fork() == 0:
	time.sleep(1)
	sendp(eth, iface=SRC_IF)
	os._exit(0)

ans=sniff(iface=SRC_IF, timeout=3, filter=
    "ip6 and src "+DST_IN6+" and dst "+SRC_OUT6+" and icmp6")
for a in ans:
	if a and a.type == ETH_P_IPV6 and \
	    ipv6nh[a.payload.nh] == 'ICMPv6' and \
	    icmp6types[a.payload.payload.type] == 'Neighbor Advertisement':
		tgt=a.payload.payload.tgt
		print "target=%s" % (tgt)
		if tgt == DST_IN6:
			exit(0)
		print "TARGET!=%s" % (DST_IN6)
		exit(1)
print "NO NEIGHBOR ADVERTISEMENT"
exit(2)
