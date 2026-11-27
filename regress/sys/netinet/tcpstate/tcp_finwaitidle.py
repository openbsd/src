#!/usr/local/bin/python3
# transfer peer into FIN_WAIT_2 state after close, check idle timeout

import os
from addr import *
from scapy.all import *

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

print("Send SYN packet, receive SYN+ACK.")
syn=TCP(sport=tport, dport='daytime', flags='S', seq=1, window=(2**16)-1)
synack=sr1(ip/syn, timeout=5)
if synack is None:
	print("ERROR: No SYN+ACK from daytime server received.")
	exit(1)

print("Send ACK packet to finish handshake, receive data.")
ack=TCP(sport=synack.dport, dport=synack.sport, flags='A',
    seq=2, ack=synack.seq+1, window=(2**16)-1)
data=sr1(ip/ack, timeout=5)
if data is None:
	print("ERROR: No data received from daytime server.")
	exit(1)
if data.getlayer(TCP).flags != 'PA':
	print("ERROR: expecting PSH, got flag '%s' in data." % \
	    (data.getlayer(TCP).flags))
	exit(1)
tcplen = data.len - data.ihl * 4 - data.dataofs * 4

print("Send ACK for data packet, receive FIN.")
data_ack=TCP(sport=synack.dport, dport=synack.sport, flags='A',
    seq=2, ack=data.seq+tcplen, window=(2**16)-1)
recv_fin=sr1(ip/data_ack, timeout=5)
if recv_fin is None:
	print("ERROR: No FIN received from daytime server.")
	exit(1)
if recv_fin.getlayer(TCP).flags != 'FA':
	print("ERROR: expecting FIN, got flag '%s' in recv FIN." % \
	    (recv_fin.getlayer(TCP).flags))
	exit(1)

print("Send ACK for FIN packet, but never send own FIN.")
send_ack=TCP(sport=synack.dport, dport=synack.sport, flags='A',
    seq=2, ack=recv_fin.seq+1, window=(2**16)-1)
send(ip/send_ack)

# keepidle=3 keepintvl=1, idle timeout is 8 * keepintvl
time.sleep(2)

print("Check peer is in FIN_WAIT_2 state.")
state = netstat_state("netstat-finwaitidle-before.log")
if state is None or "FIN_WAIT_2" not in state:
	print("ERROR: expecting FIN_WAIT_2, got %s." % (state))
	exit(1)

time.sleep(12)

print("Check peer has dropped FIN_WAIT_2 state.")
state = netstat_state("netstat-finwaitidle-after.log")

print("Send reset to cleanup the connection.")
rst=TCP(sport=synack.dport, dport=synack.sport, flags='RA',
    seq=recv_fin.ack, ack=recv_fin.seq+1)
send(ip/rst)

if state is not None:
	print("ERROR: expecting no connection, got %s." % (state))
	exit(1)

exit(0)
