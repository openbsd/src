#!/usr/local/bin/python2.7

import os
from addr import *
from scapy.all import *

ip=IP(src=FAKE_NET_ADDR, dst=REMOTE_ADDR)
tport=os.getpid() & 0xffff

print "Send SYN packet, receive SYN+ACK."
syn=TCP(sport=tport, dport='chargen', seq=1, flags='S', window=(2**16)-1)
synack=sr1(ip/syn, iface=LOCAL_IF, timeout=5)

if synack is None:
	print "ERROR: no SYN+ACK from chargen server received"
	exit(1)

print "Send ACK packet, receive chargen data."
ack=TCP(sport=synack.dport, dport=synack.sport, seq=2, flags='A',
    ack=synack.seq+1, window=(2**16)-1)
data=sr1(ip/ack, iface=LOCAL_IF, timeout=5)

if data is None:
	print "ERROR: no data from chargen server received"
	exit(1)

print "Fill our receive buffer."
time.sleep(1)

print "Send ICMP fragmentation needed packet with MTU 1300."
icmp=ICMP(type="dest-unreach", code="fragmentation-needed",
    nexthopmtu=1300)/data
# sr1 cannot be used, TCP data will not match outgoing ICMP packet
if os.fork() == 0:
	time.sleep(1)
	send(IP(src=LOCAL_ADDR, dst=REMOTE_ADDR)/icmp, iface=LOCAL_IF)
	os._exit(0)

print "Path MTU discovery will resend first data with length 1300."
ans=sniff(iface=LOCAL_IF, timeout=3, count=1, filter=
    "ip and src %s and tcp port %u and dst %s and tcp port %u" %
    (ip.dst, syn.dport, ip.src, syn.sport))

if len(ans) == 0:
	print "ERROR: no data retransmit from chargen server received"
	exit(1)
data=ans[0]

print "Cleanup the other's socket with a reset packet."
rst=TCP(sport=synack.dport, dport=synack.sport, seq=2, flags='AR',
    ack=synack.seq+1)
send(ip/rst, iface=LOCAL_IF)

len = data.len
print "len=%d" % len
if len != 1300:
	print "ERROR: TCP data packet len is %d, expected 1300." % len
	exit(1)

exit(0)
