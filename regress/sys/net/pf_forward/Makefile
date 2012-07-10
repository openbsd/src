#	$OpenBSD: Makefile,v 1.3 2012/07/10 16:58:29 bluhm Exp $

# The following ports must be installed:
#
# python-2.7          interpreted object-oriented programming language
# py-libdnet          python interface to libdnet
# scapy               powerful interactive packet manipulation in python

# This test needs a manual setup of four machines
# The setup is the same as for regress/sys/net/pf_fragment
# Set up machines: SRC PF RT DST
# SRC is the machine where this makefile is running.
# PF is running OpenBSD forwarding through pf, it is the test target.
# RT is a router forwarding packets, maximum MTU is 1300.
# DST is reflecting the ping and UDP and TCP echo packets.
# RDR does not exist, PF redirects the traffic to DST.
# AF does not exist, PF translates address family and sends to DST.
#
# +---+   1   +--+   2   +--+   3   +---+   4   +---+  5  +--+
# |SRC| ----> |PF| ----> |RT| ----> |DST|       |RDR|     |AF|
# +---+       +--+       +--+       +---+       +---+     +--+
#     out    in  out    in  out    in          in        in

# Configure Addresses on the machines, there must be routes for the networks.
# Adapt interface and addresse variables to your local setup.
#
SRC_IF =	tun0
SRC_MAC =	fe:e1:ba:d1:0a:dc
PF_MAC =	52:54:00:12:34:50

SRC_OUT =	10.188.211.10
PF_IN =		10.188.211.50
PF_OUT =	10.188.212.50
RT_IN =		10.188.212.51
RT_OUT =	10.188.213.51
DST_IN =	10.188.213.52
RDR_IN =	10.188.214.1
AF_IN =		10.188.215.82

SRC_OUT6 =	fdd7:e83e:66bc:211:fce1:baff:fed1:561f
PF_IN6 =	fdd7:e83e:66bc:211:5054:ff:fe12:3450
PF_OUT6 =	fdd7:e83e:66bc:212:5054:ff:fe12:3450
RT_IN6 =	fdd7:e83e:66bc:212:5054:ff:fe12:3451
RT_OUT6 =	fdd7:e83e:66bc:213:5054:ff:fe12:3451
DST_IN6 =	fdd7:e83e:66bc:213:5054:ff:fe12:3452
RDR_IN6 =	fdd7:e83e:66bc:214::1
AF_IN6 =	fdd7:e83e:66bc:215:5054:ff:fe12:3434

# pf rules on PF must look like this:
#
# pass to { $PF_IN/24 $PF_IN6/64 }
# pass to { $RT_IN/24 $RT_IN6/64 }
# pass to { $DST_IN/24 $DST_IN6/64 }
# pass to { $RDR_IN/24 $RDR_IN6/64 }
#
# pass in  to $RDR_IN/24  rdr-to $DST_IN  tag rdr
# pass out                nat-to $PF_OUT  tagged rdr
# pass in  to $RDR_IN6/64 rdr-to $DST_IN6 tag rdr
# pass out                nat-to $PF_OUT6 tagged rdr

# pass in  to $AF_IN/24  af-to inet6 from $PF_OUT6 to $DST_IN6/120 tag af
# pass out                     inet6                               tagged af
# pass in  to $AF_IN6/64 af-to inet  from $PF_OUT  to $DST_IN/24   tag af
# pass out                     inet                                tagged af

# Currently these test fail as pf does not fix the checksum of
# NATed packets inside of icmp packets.
# ping6-mtu

depend: addr.py

# Create python include file containing the addresses.
addr.py: Makefile
	rm -f $@ $@.tmp
	echo 'SRC_IF="${SRC_IF}"' >>$@.tmp
	echo 'SRC_MAC="${SRC_MAC}"' >>$@.tmp
	echo 'PF_MAC="${PF_MAC}"' >>$@.tmp
.for var in SRC_OUT PF_IN PF_OUT RT_IN RT_OUT DST_IN RDR_IN AF_IN
	echo '${var}="${${var}}"' >>$@.tmp
	echo '${var}6="${${var}6}"' >>$@.tmp
.endfor
	mv $@.tmp $@

# Make sure that the routing table on the SRC machine is correct.
# All packets must be forwarded to PF target machine.
TARGETS +=	route route6

run-regress-route:
	@echo '\n======== $@ ========'
	@echo Check route SRC_OUT:
	route -n get -inet ${SRC_OUT} | fgrep -q 'interface: lo0'
	@echo Check route PF_IN:
	route -n get -inet ${PF_IN} | fgrep -q 'if address: ${SRC_OUT}'
.for ip in PF_OUT RT_IN RT_OUT DST_IN RDR_IN AF_IN
	@echo Check route ${ip}:
	route -n get -inet ${${ip}} | fgrep -q 'gateway: ${PF_IN}'
.endfor

run-regress-route6:
	@echo '\n======== $@ ========'
	@echo Check route SRC_OUT6:
	route -n get -inet6 ${SRC_OUT6} | fgrep -q 'interface: lo0'
.for ip in PF_OUT RT_IN RT_OUT DST_IN RDR_IN AF_IN
	@echo Check route ${ip}6:
	route -n get -inet6 ${${ip}6} | fgrep -q 'gateway: ${PF_IN6}'
.endfor

# Ping all addresses.  This ensures that the ip addresses are configured
# and all routing table are set up to allow bidirectional packet flow.
# Note that RDR does not exist physically.  So this traffic is rewritten
# by PF and handled by DST.
TARGETS +=	ping  ping6

run-regress-ping:
	@echo '\n======== $@ ========'
.for ip in SRC_OUT PF_IN PF_OUT RT_IN RT_OUT DST_IN RDR_IN AF_IN
	@echo Check ping ${ip}:
	ping -n -c 1 ${${ip}}
.endfor

run-regress-ping6:
	@echo '\n======== $@ ========'
.for ip in SRC_OUT PF_IN PF_OUT RT_IN RT_OUT DST_IN RDR_IN AF_IN
	@echo Check ping ${ip}6:
	ping6 -n -c 1 ${${ip}6}
.endfor

# Send a large IPv4/ICMP-Echo-Request packet with enabled DF bit and
# parse response packet to determine MTU of the router.  The MTU has
# to be 1300 octets.  The MTU has to be defined at out interface of
# the router RT before.
# Check that the ip length of the original packet and the icmp
# quoted packet are the same.
TARGETS +=	ping-mtu ping6-mtu

run-regress-ping-mtu: addr.py
	@echo '\n======== $@ ========'
.for ip in DST_IN RDR_IN
	@echo Check path MTU to ${ip} is 1300
	${SUDO} python2.7 ping_mtu.py ${${ip}} 1300
.endfor
	@echo Check path MTU to AF_IN is 1280
	${SUDO} python2.7 ping_mtu.py ${AF_IN} 1280

run-regress-ping6-mtu: addr.py
	@echo '\n======== $@ ========'
.for ip in DST_IN RDR_IN
	@echo Check path MTU to ${ip}6 is 1300
	${SUDO} python2.7 ping6_mtu.py ${${ip}6} 1300
.endfor
	@echo Check path MTU to AF_IN6 is 1320
	${SUDO} python2.7 ping6_mtu.py ${AF_IN6} 1320

# Send one udp echo port 7 packet to all destination addresses with netcat.
# The response must arrive in 1 second.
TARGETS +=	udp  udp6

run-regress-udp:
	@echo '\n======== $@ ========'
.for ip in DST_IN RDR_IN AF_IN
	@echo Check udp ${ip}:
	( echo $$$$ | nc -u ${${ip}} 7 & sleep 1; kill $$! ) | grep $$$$
.endfor

run-regress-udp6:
	@echo '\n======== $@ ========'
.for ip in DST_IN RDR_IN AF_IN
	@echo Check udp ${ip}6:
	( echo $$$$ | nc -u ${${ip}6} 7 & sleep 1; kill $$! ) | grep $$$$
.endfor

# Send a data stream to tcp echo port 7 to all destination addresses
# with netcat.  Use enough data to make sure PMTU discovery works.
# Count the reflected bytes and compare with the transmitted ones.
TARGETS +=	tcp  tcp6

run-regress-tcp:
	@echo '\n======== $@ ========'
.for ip in DST_IN RDR_IN AF_IN
	@echo Check tcp ${ip}:
	openssl rand 200000 | nc ${${ip}} 7 | wc -c | grep '200000$$'
.endfor

run-regress-tcp6:
	@echo '\n======== $@ ========'
.for ip in DST_IN RDR_IN AF_IN
	@echo Check tcp ${ip}6:
	openssl rand 200000 | nc ${${ip}6} 7 | wc -c | grep '200000$$'
.endfor

REGRESS_TARGETS =	${TARGETS:S/^/run-regress-/}

CLEANFILES +=		addr.py *.pyc *.log

.include <bsd.regress.mk>
