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

if synack is None:
	print "ERROR: no SYN+ACK from chargen server received"
	exit(1)

print "Send ACK packet, receive chargen data."
ack=TCP(sport=synack.dport, dport=synack.sport, seq=2, flags='A',
    ack=synack.seq+1, window=(2**16)-1)
data=srp1(e/ip6/ack, iface=LOCAL_IF, timeout=5)

if data is None:
	print "ERROR: no data from chargen server received"
	exit(1)

print "Fill our receive buffer."
time.sleep(1)

print "Send ICMP6 packet too big packet with MTU 1272."
icmp6=ICMPv6PacketTooBig(mtu=1272)/data.payload
sendp(e/IPv6(src=LOCAL_ADDR6, dst=REMOTE_ADDR6)/icmp6, iface=LOCAL_IF)

print "Path MTU discovery will resend first data with length 1272."
# srp1 cannot be used, fragment answer will not match on outgoing udp packet
if os.fork() == 0:
	time.sleep(1)
	sendp(e/ip6/ack, iface=LOCAL_IF)
	os._exit(0)

ans=sniff(iface=LOCAL_IF, timeout=3, filter=
    "ip6 and src "+ip6.dst+" and dst "+ip6.src+" and proto ipv6-frag")

for a in ans:
	fh=a.payload.payload
	if fh.offset != 0 or fh.nh != (ip6/syn).nh:
		continue
	th=fh.payload
	if th.sport != syn.dport or th.dport != syn.sport:
		continue
	frag=a
	break
else:
	print "ERROR: no fragement retransmit from chargen server received"
	exit(1)

print "Cleanup the other's socket with a reset packet."
rst=TCP(sport=synack.dport, dport=synack.sport, seq=2, flags='AR',
    ack=synack.seq+1)
sendp(e/ip6/rst, iface=LOCAL_IF)

if frag.offset != 0:
	print "ERROR: TCP fragment is not atomic, offset is %d." % frag.offset
	exit(1)

if frag.m != 0:
	print "ERROR: TCP fragment is not atomic, more fragment bit is set."
	exit(1)

print "Atomic fragment contains 8 octet header, so expected len is 1280"
flen = frag.plen + len(IPv6())
print "flen=%d" % flen
if flen != 1280:
	print "ERROR: TCP atomic fragment len is %d, expected 1280." % flen
	exit(1)

exit(0)
