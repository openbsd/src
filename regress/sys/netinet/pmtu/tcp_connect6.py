#!/usr/local/bin/python2.7

import os
from addr import *
from scapy.all import *

e=Ether(src=LOCAL_MAC, dst=REMOTE_MAC)
ip6=IPv6(src=FAKE_NET_ADDR6, dst=REMOTE_ADDR6)
port=os.getpid() & 0xffff

print "Send SYN packet, receive SYN+ACK."
syn=TCP(sport=port, dport='chargen', seq=1, flags='S', window=(2**16)-1)
synack=srp1(e/ip6/syn, iface=LOCAL_IF, timeout=5)

print "Send ack packet, receive chargen data."
ack=TCP(sport=synack.dport, dport=synack.sport, seq=2, flags='A',
    ack=synack.seq+1, window=(2**16)-1)
data=srp1(e/ip6/ack, iface=LOCAL_IF, timeout=5)

print "Fill our receive buffer."
time.sleep(1)

print "Send ICMP6 packet too big packet with MTU 1300."
icmp6=ICMPv6PacketTooBig(mtu=1300)/data.payload
sendp(e/IPv6(src=LOCAL_ADDR6, dst=REMOTE_ADDR6)/icmp6, iface=LOCAL_IF)

print "Path MTU discovery will resend first data with length 1300."
data=srp1(e/ip6/ack, iface=LOCAL_IF, timeout=5)

print "Cleanup the other's socket with a reset packet."
rst=TCP(sport=synack.dport, dport=synack.sport, seq=2, flags='AR',
    ack=synack.seq+1)
sendp(e/ip6/rst, iface=LOCAL_IF)

len = data.plen + len(IPv6())
print "len=%d" % len
if len != 1300:
	print "ERROR: TCP data packet len is %d, expected 1300." % len
	exit(1)
exit(0)
