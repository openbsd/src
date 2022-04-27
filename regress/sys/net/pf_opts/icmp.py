#!/usr/local/bin/python3

print("send icmp without options")

import os
import sys
from addr import *
from scapy.all import *

if len(sys.argv) != 2:
	print("usage: icmp.py Nn")
	exit(2)

N=sys.argv[1]
IF=eval("IF_"+N);
ADDR=eval("ADDR_"+N);

pid=os.getpid()
eid=pid & 0xffff
payload=b"ABCDEFGHIJKLMNOP"
packet=IP(src=ADDR, dst=ADDR)/ \
    ICMP(type=6, id=eid)/payload

send(packet, iface=IF)
