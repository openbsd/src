#!/usr/local/bin/python2.7
# send Unsolicited Neighbor Advertisement

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

ip=IPv6(src=lla(SRC_MAC), dst="ff02::1")/ICMPv6ND_NA(tgt=SRC_OUT6)
eth=Ether(src=SRC_MAC, dst=nsmac("ff02::1"))/ip

sendp(eth, iface=SRC_IF)
time.sleep(1)

exit(0)
