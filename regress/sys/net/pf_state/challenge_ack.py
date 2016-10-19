#!/usr/local/bin/python2.7
# check wether path mtu to dst is as expected

import os
import threading
from addr import *
from scapy.all import *

# usage: challenge_ack.py src dst

#
# we can not use scapy's sr() function as receive side
# ignores the packet we expect to see. Packet is ignored
# due to mismatching sequence numbers. 'bogus_syn' is using
# seq = 1000000, while response sent back by PF has ack,
# which fits regular session opened by 'syn'.
#
class Sniff(threading.Thread):
	captured = None
	def run(self):
		self.captured = sniff(iface=LOCAL_IF,
		    filter='tcp src port 7', timeout=3)

srcaddr=sys.argv[1]
dstaddr=sys.argv[2]
port=os.getpid() & 0xffff

ip=IP(src=srcaddr, dst=dstaddr)

print "Send SYN packet, receive SYN+ACK"
syn=TCP(sport=port, dport='echo', seq=1, flags='S', window=(2**16)-1)
synack=sr1(ip/syn, iface=LOCAL_IF, timeout=5)

print "Send ACK packet to finish handshake."
ack=TCP(sport=synack.dport, dport=synack.sport, seq=2, flags='A',
    ack=synack.seq+1)
send(ip/ack, iface=LOCAL_IF)

print "Connection is established, send bogus SYN, expect challenge ACK"
bogus_syn=TCP(sport=port, dport='echo', seq=1000000, flags='S',
    window=(2**16)-1)
sniffer = Sniff();
sniffer.start()
challenge_ack=send(ip/bogus_syn, iface=LOCAL_IF)
sniffer.join(timeout=5)

if sniffer.captured == None:
	print "ERROR: no packet received"
	exit(1)

challenge_ack = None

for p in sniffer.captured:
	if p.haslayer(TCP) and p.getlayer(TCP).sport == 7 and \
	    p.getlayer(TCP).flags == 16:
		challenge_ack = p
		break

if challenge_ack == None:
	print "No ACK has been seen"
	exit(1)

if challenge_ack.getlayer(TCP).seq != (synack.seq + 1):
	print "ERROR: expecting seq %d got %d in challange ack" % \
	    (challenge_ack.getlayer(TCP).seq, (synack.seq + 1))
	exit(1)

exit(0)
