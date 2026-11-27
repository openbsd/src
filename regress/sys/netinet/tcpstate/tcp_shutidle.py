#!/usr/local/bin/python3
# peer shutdown into FIN_WAIT_2 state, then close, check idle timeout

import os
import subprocess
import threading
from addr import *
from scapy.all import *

class Sniff1(threading.Thread):
	filter = None
	captured = None
	packet = None
	count = None
	timeout = None
	def __init__(self, count=1, timeout=3):
		self.count = count
		self.timeout = timeout
		# clear packets buffered by scapy bpf
		sniff(iface=LOCAL_IF, timeout=1)
		super(Sniff1, self).__init__()
	def run(self):
		self.captured = sniff(iface=LOCAL_IF, filter=self.filter,
		    count=self.count, timeout=self.timeout)
		if self.captured:
			self.packet = self.captured[0]

def netstat_state(name):
	found = None
	with os.popen("ssh "+REMOTE_SSH+" netstat -vnp tcp") as netstat:
		with open(name, 'w') as log:
			for line in netstat:
				if "%s.%d" % (FAKE_NET_ADDR, tport) in line:
					print(line)
					log.write(line)
					found = line.strip()
	return found

ip=IP(src=FAKE_NET_ADDR, dst=REMOTE_ADDR)
tport=os.getpid() & 0xffff

print("Start sniffer for SYN packet from peer.")
sniffer = Sniff1(timeout=10)
sniffer.filter = \
    "ip and src %s and dst %s and tcp port %u " \
    "and tcp[tcpflags] = tcp-syn" % \
    (ip.dst, ip.src, tport)
sniffer.start()
time.sleep(1)

print("Connect from remote client.")
client=subprocess.Popen("ssh %s perl %s/client.pl %s %s %u shutdown" % \
    (REMOTE_SSH, CURDIR, ip.dst, ip.src, tport), shell=True,
    stdin=subprocess.PIPE)

print("Wait for SYN.")
sniffer.join(timeout=5)
syn=sniffer.packet
if syn is None:
	print("ERROR: No SYN received from remote client.")
	exit(1)

synack=TCP(sport=syn.dport, dport=syn.sport, flags='SA',
    seq=1, ack=syn.seq+1, window=(2**16)-1)
ack=sr1(ip/synack, timeout=5)
if ack is None:
	print("ERROR: No ACK from remote client received.")
	exit(1)

print("Start sniffer for FIN packet from peer.")
sniffer = Sniff1(timeout=10)
sniffer.filter = \
    "ip and src %s and dst %s and tcp port %u " \
    "and tcp[tcpflags] = tcp-fin|tcp-ack" % \
    (ip.dst, ip.src, tport)
sniffer.start()
time.sleep(1)

print("Close stdin of remote client to shutdown its socket.")
client.stdin.close()

print("Wait for FIN.")
sniffer.join(timeout=10)
recv_fin=sniffer.packet
if recv_fin is None:
	print("ERROR: No FIN received from remote client.")
	exit(1)

print("Send ACK for FIN packet to reach FIN_WAIT_2.")
send_ack=TCP(sport=syn.dport, dport=syn.sport, flags='A',
    seq=2, ack=recv_fin.seq+1, window=(2**16)-1)
send(ip/send_ack)
time.sleep(1)

# Readable socket wakes up select(2), client closes in FIN_WAIT_2.
print("Send data to make remote client close its socket.")
data=TCP(sport=syn.dport, dport=syn.sport, flags='PA',
    seq=2, ack=recv_fin.seq+1, window=(2**16)-1)/b"a"
send(ip/data)

if client.wait(timeout=10) != 0:
	print("ERROR: Remote client failed.")
	exit(1)

# keepidle=3 keepintvl=1, idle timeout is 8 * keepintvl
time.sleep(1)

print("Check peer is in FIN_WAIT_2 state.")
state = netstat_state("netstat-shutidle-before.log")
if state is None or "FIN_WAIT_2" not in state:
	print("ERROR: expecting FIN_WAIT_2, got %s." % (state))
	exit(1)

time.sleep(12)

print("Check peer has dropped FIN_WAIT_2 state.")
state = netstat_state("netstat-shutidle-after.log")

print("Send reset to cleanup the connection.")
rst=TCP(sport=syn.dport, dport=syn.sport, flags='RA',
    seq=recv_fin.ack+1, ack=recv_fin.seq+1)
send(ip/rst)

if state is not None:
	print("ERROR: expecting no connection, got %s." % (state))
	exit(1)

exit(0)
