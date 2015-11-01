#	$OpenBSD: Makefile,v 1.14 2015/11/01 21:30:00 bluhm Exp $

# The following ports must be installed:
#
# python-2.7          interpreted object-oriented programming language
# py-libdnet          python interface to libdnet
# scapy               powerful interactive packet manipulation in python

# Check wether all required python packages are installed.  If some
# are missing print a warning and skip the tests, but do not fail.
PYTHON_IMPORT != python2.7 -c 'from scapy.all import *' 2>&1 || true
.if ! empty(PYTHON_IMPORT)
regress:
	@echo '${PYTHON_IMPORT}'
	@echo install python and the scapy module for additional tests
.endif

# This test needs a manual setup of four machines
# The setup is the same as for regress/sys/net/pf_fragment
# Set up machines: SRC PF RT ECO
# SRC is the machine where this makefile is running.
# PF is running OpenBSD forwarding through pf, it is the test target.
# RT is a router forwarding packets, maximum MTU is 1300.
# ECO is reflecting the ping and UDP and TCP echo packets.
# RDR does not exist, PF redirects the traffic to ECO.
# AF does not exist, PF translates address family and sends to ECO.
# RTT addresses exist on ECO, PF has no route and must use route-to RT
# RPT addresses exist on SRC, PF has no route and must use reply-to SRC
#
# +---+   0   +--+   1   +--+   2   +---+ 3
# |SRC| ----> |PF| ----> |RT| ----> |ECO| 7
# +---+ 8     +--+       +--+       +---+ 9
#     out    in  out    in  out    in   out
#
# 4 +---+ 5   6 +--+   7 +---+   +---+ 8
#   |RDR|       |AF|     |RTT|   |RPT|
#   +---+       +--+     +---+   +---+
#  in   out    in       in           out

# Configure Addresses on the machines, there must be routes for the
# networks.  Adapt interface and addresse variables to your local
# setup.  To control the remote machine you need a hostname for
# ssh to log in.
# You must have an anchor "regress" for the divert rules in the pf.conf
# of the PF machine.  The kernel of the PF machine gets testet.
#
# Run make check-setup to see if you got the setup correct.

SRC_IF ?=	tap0
SRC_MAC ?=	fe:e1:ba:d1:0a:dc
PF_IFIN ?=	vio0
PF_IFOUT ?=	vio1
PF_MAC ?=	52:54:00:12:34:50
PF_SSH ?=
RT_SSH ?=
ECO_SSH ?=

SRC_OUT ?=	10.188.210.10
PF_IN ?=	10.188.210.50
PF_OUT ?=	10.188.211.50
RT_IN ?=	10.188.211.51
RT_OUT ?=	10.188.212.51
ECO_IN ?=	10.188.212.52
ECO_OUT ?=	10.188.213.52
RDR_IN ?=	10.188.214.188
RDR_OUT ?=	10.188.215.188
AF_IN ?=	10.188.216.82		# /24 must be dec(ECO_IN6/120)
RTT_IN ?=	10.188.217.52
RPT_OUT ?=	10.188.218.10

SRC_OUT6 ?=	fdd7:e83e:66bc:210:fce1:baff:fed1:561f
PF_IN6 ?=	fdd7:e83e:66bc:210:5054:ff:fe12:3450
PF_OUT6 ?=	fdd7:e83e:66bc:211:5054:ff:fe12:3450
RT_IN6 ?=	fdd7:e83e:66bc:211:5054:ff:fe12:3451
RT_OUT6 ?=	fdd7:e83e:66bc:212:5054:ff:fe12:3451
ECO_IN6 ?=	fdd7:e83e:66bc:212:5054:ff:fe12:3452
ECO_OUT6 ?=	fdd7:e83e:66bc:213:5054:ff:fe12:3452
RDR_IN6 ?=	fdd7:e83e:66bc:214::188
RDR_OUT6 ?=	fdd7:e83e:66bc:215::188
AF_IN6 ?=	fdd7:e83e:66bc:216::34	# /120 must be hex(ECO_IN/24)
RTT_IN6 ?=	fdd7:e83e:66bc:217:5054:ff:fe12:3452
RPT_OUT6 ?=	fdd7:e83e:66bc:1218:fce1:baff:fed1:561f

.if empty (PF_SSH) || empty (RT_SSH) || empty (ECO_SSH)
regress:
	@echo this tests needs three remote machines to operate on
	@echo PF_SSH RT_SSH ECO_SSH are empty
	@echo fill out these variables for additional tests, then
	@echo check wether your test machines are set up properly
.endif

.MAIN: all

.if ! empty (PF_SSH)
.if make (regress) || make (all)
.BEGIN: pf.conf addr.py
	@echo
	${SUDO} true
	ssh -t ${PF_SSH} ${SUDO} true
	rm -f stamp-pfctl
.endif
.endif

depend: addr.py

# Create python include file containing the addresses.
addr.py: Makefile
	rm -f $@ $@.tmp
	echo 'SRC_IF="${SRC_IF}"' >>$@.tmp
	echo 'SRC_MAC="${SRC_MAC}"' >>$@.tmp
	echo 'PF_IFIN="${PF_IFIN}"' >>$@.tmp
	echo 'PF_IFOUT="${PF_IFOUT}"' >>$@.tmp
	echo 'PF_MAC="${PF_MAC}"' >>$@.tmp
.for var in SRC_OUT PF_IN PF_OUT RT_IN RT_OUT ECO_IN ECO_OUT RDR_IN RDR_OUT AF_IN RTT_IN RPT_OUT
	echo '${var}="${${var}}"' >>$@.tmp
	echo '${var}6="${${var}6}"' >>$@.tmp
.endfor
	mv $@.tmp $@

# load the pf rules into the kernel of the PF machine
# XXX pfctl does not replace variables after @
stamp-pfctl: addr.py pf.conf
	cat addr.py ${.CURDIR}/pf.conf | pfctl -n -f -
	cat addr.py ${.CURDIR}/pf.conf | \
	    sed 's/@$$PF_IFIN /@${PF_IFIN} /;s/@$$PF_IFOUT /@${PF_IFOUT} /' | \
	    ssh ${PF_SSH} ${SUDO} pfctl -a regress -f -
	@date >$@

# Set variables so that make runs with and without obj directory.
# Only do that if necessary to keep visible output short.
.if ${.CURDIR} == ${.OBJDIR}
PYTHON =	python2.7 ./
.else
PYTHON =	PYTHONPATH=${.OBJDIR} python2.7 ${.CURDIR}/
.endif

# Ping all addresses.  This ensures that the IP addresses are configured
# and all routing table are set up to allow bidirectional packet flow.
# Note that RDR does not exist physically.  So this traffic is rewritten
# by PF and handled by ECO.
TARGETS +=	ping  ping6

run-regress-ping: stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in SRC_OUT PF_IN PF_OUT RT_IN RT_OUT ECO_IN ECO_OUT RDR_IN RDR_OUT AF_IN RTT_IN
	@echo Check ping ${ip}:
	ping -n -c 1 ${${ip}}
.endfor
	@echo Check ping RPT_OUT:
	ping -n -c 1 -I ${RPT_OUT} ${ECO_IN}

run-regress-ping6: stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in SRC_OUT PF_IN PF_OUT RT_IN RT_OUT ECO_IN ECO_OUT RDR_IN RDR_OUT AF_IN RTT_IN
	@echo Check ping ${ip}6:
	ping6 -n -c 1 ${${ip}6}
.endfor
	@echo Check ping RPT_OUT6:
	ping6 -n -c 1 -I ${RPT_OUT6} ${ECO_IN6}

# Send a large IPv4/ICMP-Echo-Request packet with enabled DF bit and
# parse response packet to determine MTU of the packet filter.  The
# outgoing MTU of PF has to be 1400 octets.  Packet size is 1500.
# Check that the IP length of the original packet and the ICMP
# quoted packet are the same.
# XXX AF_IN is broken with PF MTU
TARGETS +=	ping-mtu-1400 ping6-mtu-1400

run-regress-ping-mtu-1400: addr.py stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN ECO_OUT RDR_IN RDR_OUT RTT_IN
	@echo Check path MTU to ${ip} is 1400
	${SUDO} ${PYTHON}ping_mtu.py ${SRC_OUT} ${${ip}} 1500 1400
.endfor
	@echo Check path MTU from RPT_OUT is 1400
	${SUDO} ${PYTHON}ping_mtu.py ${RPT_OUT} ${ECO_IN} 1500 1400

run-regress-ping6-mtu-1400: addr.py stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN ECO_OUT RDR_IN RDR_OUT RTT_IN
	@echo Check path MTU to ${ip}6 is 1400
	${SUDO} ${PYTHON}ping6_mtu.py ${SRC_OUT6} ${${ip}6} 1500 1400
.endfor
	@echo Check path MTU from RPT_OUT6 is 1400
	${SUDO} ${PYTHON}ping6_mtu.py ${RPT_OUT6} ${ECO_IN6} 1500 1400

# Send a large IPv4/ICMP-Echo-Request packet with enabled DF bit and
# parse response packet to determine MTU of the router.  The MTU has
# to be 1300 octets.  The MTU has to be defined at out interface of
# the router RT before.  Packet size is 1400 to pass PF MTU.
# Check that the IP length of the original packet and the ICMP
# quoted packet are the same.
TARGETS +=	ping-mtu-1300 ping6-mtu-1300

run-regress-ping-mtu-1300: addr.py stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN ECO_OUT RDR_IN RDR_OUT RTT_IN
	@echo Check path MTU to ${ip} is 1300
	${SUDO} ${PYTHON}ping_mtu.py ${SRC_OUT} ${${ip}} 1400 1300
.endfor
	@echo Check path MTU to AF_IN is 1280
	${SUDO} ${PYTHON}ping_mtu.py ${SRC_OUT} ${AF_IN} 1380 1280
	@echo Check path MTU from RPT_OUT is 1300
	${SUDO} ${PYTHON}ping_mtu.py ${RPT_OUT} ${ECO_IN} 1400 1300

run-regress-ping6-mtu-1300: addr.py stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN ECO_OUT RDR_IN RDR_OUT RTT_IN
	@echo Check path MTU to ${ip}6 is 1300
	${SUDO} ${PYTHON}ping6_mtu.py ${SRC_OUT6} ${${ip}6} 1400 1300
.endfor
	@echo Check path MTU to AF_IN6 is 1320
	${SUDO} ${PYTHON}ping6_mtu.py ${SRC_OUT6} ${AF_IN6} 1420 1320
	@echo Check path MTU from RPT_OUT6 is 1300
	${SUDO} ${PYTHON}ping6_mtu.py ${RPT_OUT6} ${ECO_IN6} 1400 1300

# Send one UDP echo port 7 packet to all destination addresses with netcat.
# The response must arrive in 1 second.
TARGETS +=	udp  udp6

run-regress-udp: stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN ECO_OUT RDR_IN RDR_OUT AF_IN RTT_IN
	@echo Check UDP ${ip}:
	( echo $$$$ | nc -u ${${ip}} 7 & sleep 1; kill $$! ) | grep $$$$
.endfor
	@echo Check UDP RPT_OUT:
	( echo $$$$ | nc -u -s ${RPT_OUT} ${ECO_IN} 7 & sleep 1; kill $$! ) | grep $$$$

run-regress-udp6: stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN ECO_OUT RDR_IN RDR_OUT AF_IN RTT_IN
	@echo Check UDP ${ip}6:
	( echo $$$$ | nc -u ${${ip}6} 7 & sleep 1; kill $$! ) | grep $$$$
.endfor
	@echo Check UDP RPT_OUT6:
	( echo $$$$ | nc -u -s ${RPT_OUT6} ${ECO_IN6} 7 & sleep 1; kill $$! ) | grep $$$$

# Send a data stream to TCP echo port 7 to all destination addresses
# with netcat.  Use enough data to make sure PMTU discovery works.
# Count the reflected bytes and compare with the transmitted ones.
# Delete host route before test to trigger PMTU discovery.
# XXX AF_IN is broken with PF MTU, make sure that it hits RT MTU 1300.
TARGETS +=	tcp  tcp6

run-regress-tcp: stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN ECO_OUT RDR_IN RDR_OUT RTT_IN
	@echo Check tcp ${ip}:
	${SUDO} route -n delete -host -inet ${${ip}} || true
	openssl rand 200000 | nc -N ${${ip}} 7 | wc -c | grep '200000$$'
.endfor
	@echo Check tcp AF_IN:
	${SUDO} route -n delete -host -inet ${AF_IN} || true
	${SUDO} ${PYTHON}ping_mtu.py ${SRC_OUT} ${AF_IN} 1380 1280 || true
	openssl rand 200000 | nc -N ${AF_IN} 7 | wc -c | grep '200000$$'
	@echo Check tcp RPT_OUT:
	${SUDO} route -n delete -host -inet ${RPT_OUT} || true
	openssl rand 200000 | nc -N -s ${RPT_OUT} ${ECO_IN} 7 | wc -c | grep '200000$$'

run-regress-tcp6: stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN ECO_OUT RDR_IN RDR_OUT RTT_IN
	@echo Check tcp ${ip}6:
	${SUDO} route -n delete -host -inet6 ${${ip}6} || true
	openssl rand 200000 | nc -N ${${ip}6} 7 | wc -c | grep '200000$$'
.endfor
	@echo Check tcp AF_IN6:
	${SUDO} route -n delete -host -inet6 ${AF_IN6} || true
	${SUDO} ${PYTHON}ping6_mtu.py ${SRC_OUT6} ${AF_IN6} 1420 1320 || true
	openssl rand 200000 | nc -N ${AF_IN6} 7 | wc -c | grep '200000$$'
	@echo Check tcp RPT_OUT6:
	${SUDO} route -n delete -host -inet6 ${RPT_OUT6} || true
	openssl rand 200000 | nc -N -s ${RPT_OUT6} ${ECO_IN6} 7 | wc -c | grep '200000$$'

REGRESS_TARGETS =	${TARGETS:S/^/run-regress-/}

CLEANFILES +=		addr.py *.pyc *.log stamp-*

.PHONY: check-setup

# Check wether the address, route and remote setup is correct
check-setup:
	@echo '\n======== $@ SRC ========'
.for ip in SRC_OUT RPT_OUT
	ping -n -c 1 ${${ip}}  # ${ip}
	route -n get -inet ${${ip}} | grep -q 'flags: .*LOCAL'  # ${ip}
.endfor
	ping -n -c 1 ${PF_IN}  # PF_IN
	route -n get -inet ${PF_IN} | fgrep -q 'interface: ${SRC_IF}'  # PF_IN SRC_IF
.for ip in PF_OUT RT_IN RT_OUT ECO_IN ECO_OUT RDR_IN RDR_OUT AF_IN RTT_IN
	route -n get -inet ${${ip}} | fgrep -q 'gateway: ${PF_IN}'  # ${ip} PF_IN
.endfor
.for ip in SRC_OUT RPT_OUT
	ping6 -n -c 1 ${${ip}6}  # ${ip}6
	route -n get -inet6 ${${ip}6} | grep -q 'flags: .*LOCAL'  # ${ip}6
.endfor
	ping6 -n -c 1 ${PF_IN6}  # PF_IN6
	route -n get -inet6 ${PF_IN6} | fgrep -q 'interface: ${SRC_IF}'  # PF_IN6 SRC_IF
.for ip in PF_OUT RT_IN RT_OUT ECO_IN ECO_OUT RDR_IN RDR_OUT AF_IN RTT_IN
	route -n get -inet6 ${${ip}6} | fgrep -q 'gateway: ${PF_IN6}'  # ${ip}6 PF_IN6
.endfor
	@echo '\n======== $@ PF ========'
	ssh ${PF_SSH} ping -n -c 1 ${PF_IN}  # PF_IN
	ssh ${PF_SSH} route -n get -inet ${PF_IN} | grep -q 'flags: .*LOCAL'  # PF_IN
	ssh ${PF_SSH} ping -n -c 1 ${SRC_OUT}  # SRC_OUT
	ssh ${PF_SSH} ping -n -c 1 ${PF_OUT}  # PF_OUT
	ssh ${PF_SSH} route -n get -inet ${PF_OUT} | grep -q 'flags: .*LOCAL'  # PF_OUT
	ssh ${PF_SSH} ping -n -c 1 ${RT_IN}  # RT_IN
.for ip in RT_OUT ECO_IN ECO_OUT
	ssh ${PF_SSH} route -n get -inet ${${ip}} | fgrep -q 'gateway: ${RT_IN}'  # ${ip} RT_IN
.endfor
.for ip in RTT_IN RPT_OUT
	ssh ${PF_SSH} route -n get -inet ${${ip}} | grep -q 'flags: .*REJECT'  # ${ip} reject
.endfor
	ssh ${PF_SSH} ping6 -n -c 1 ${PF_IN6}  # PF_IN6
	ssh ${PF_SSH} route -n get -inet6 ${PF_IN6} | grep -q 'flags: .*LOCAL'  # PF_IN6
	ssh ${PF_SSH} ping6 -n -c 1 ${SRC_OUT6}  # SRC_OUT6
	ssh ${PF_SSH} ping6 -n -c 1 ${PF_OUT6}  # PF_OUT6
	ssh ${PF_SSH} route -n get -inet6 ${PF_OUT6} | grep -q 'flags: .*LOCAL'  # PF_OUT6
	ssh ${PF_SSH} ping6 -n -c 1 ${RT_IN6}  # RT_IN6
.for ip in RT_OUT ECO_IN ECO_OUT
	ssh ${PF_SSH} route -n get -inet6 ${${ip}6} | fgrep -q 'gateway: ${RT_IN6}'  # ${ip}6 RT_IN6
.endfor
.for ip in RTT_IN RPT_OUT
	ssh ${PF_SSH} route -n get -inet6 ${${ip}6} | grep -q 'flags: .*REJECT'  # ${ip}6 reject
.endfor
	ssh ${PF_SSH} ${SUDO} pfctl -sr | grep '^anchor "regress" all$$'
	ssh ${PF_SSH} ${SUDO} pfctl -si | grep '^Status: Enabled '
	ssh ${PF_SSH} sysctl net.inet.ip.forwarding | fgrep =1
	ssh ${PF_SSH} sysctl net.inet6.ip6.forwarding | fgrep =1
	ssh ${PF_SSH} ifconfig ${PF_IFOUT} | fgrep 'mtu 1400'
	@echo '\n======== $@ RT ========'
	ssh ${RT_SSH} ping -n -c 1 ${RT_IN}  # RT_IN
	ssh ${RT_SSH} route -n get -inet ${RT_IN} | grep -q 'flags: .*LOCAL'  # RT_IN
	ssh ${RT_SSH} ping -n -c 1 ${PF_OUT}  # PF_OUT
.for ip in PF_IN SRC_OUT RPT_OUT
	ssh ${RT_SSH} route -n get -inet ${${ip}} | fgrep -q 'gateway: ${PF_OUT}'  # ${ip} PF_OUT
.endfor
	ssh ${RT_SSH} ping -n -c 1 ${RT_OUT}  # RT_OUT
	ssh ${RT_SSH} route -n get -inet ${RT_OUT} | grep -q 'flags: .*LOCAL'  # RT_OUT
	ssh ${RT_SSH} ping -n -c 1 ${ECO_IN}  # ECO_IN
.for ip in ECO_OUT RTT_IN
	ssh ${RT_SSH} route -n get -inet ${${ip}} | fgrep -q 'gateway: ${ECO_IN}'  # ${ip} ECO_IN
.endfor
	ssh ${RT_SSH} ping6 -n -c 1 ${RT_IN6}  # RT_IN6
	ssh ${RT_SSH} route -n get -inet6 ${RT_IN6} | grep -q 'flags: .*LOCAL'  # RT_IN6
	ssh ${RT_SSH} ping6 -n -c 1 ${PF_OUT6}  # PF_OUT6
.for ip in PF_IN SRC_OUT RPT_OUT
	ssh ${RT_SSH} route -n get -inet6 ${${ip}6} | fgrep -q 'gateway: ${PF_OUT6}'  # ${ip}6 PF_OUT6
.endfor
	ssh ${RT_SSH} ping6 -n -c 1 ${RT_OUT6}  # RT_OUT6
	ssh ${RT_SSH} route -n get -inet6 ${RT_OUT6} | grep -q 'flags: .*LOCAL'  # RT_OUT6
	ssh ${RT_SSH} ping6 -n -c 1 ${ECO_IN6}  # ECO_IN6
.for ip in ECO_OUT RTT_IN
	ssh ${RT_SSH} route -n get -inet6 ${${ip}6} | fgrep -q 'gateway: ${ECO_IN6}'  # ${ip}6 ECO_IN6
.endfor
	ssh ${RT_SSH} sysctl net.inet.ip.forwarding | fgrep =1
	ssh ${RT_SSH} sysctl net.inet6.ip6.forwarding | fgrep =1
	ssh ${RT_SSH} ifconfig | fgrep 'mtu 1300'
	@echo '\n======== $@ ECO ========'
.for ip in ECO_IN ECO_OUT RTT_IN
	ssh ${ECO_SSH} ping -n -c 1 ${${ip}}  # ${ip}
	ssh ${ECO_SSH} route -n get -inet ${${ip}} | grep -q 'flags: .*LOCAL'  # ${ip}
.endfor
	ssh ${ECO_SSH} ping -n -c 1 ${RT_OUT}  # RT_OUT
.for ip in RT_IN PF_OUT PF_IN SRC_OUT RPT_OUT
	ssh ${ECO_SSH} route -n get -inet ${${ip}} | fgrep -q 'gateway: ${RT_OUT}'  # ${ip} RT_OUT
.endfor
.for ip in ECO_IN ECO_OUT RTT_IN
	ssh ${ECO_SSH} ping6 -n -c 1 ${${ip}6}  # ${ip}6
	ssh ${ECO_SSH} route -n get -inet6 ${${ip}6} | grep -q 'flags: .*LOCAL'  # ${ip}6
.endfor
	ssh ${ECO_SSH} ping6 -n -c 1 ${RT_OUT6}  # RT_OUT6
.for ip in RT_IN PF_OUT PF_IN SRC_OUT RPT_OUT
	ssh ${ECO_SSH} route -n get -inet6 ${${ip}6} | fgrep -q 'gateway: ${RT_OUT6}'  # ${ip}6 RT_OUT6
.endfor
.for af in inet inet6
.for proto in udp tcp
	ssh ${ECO_SSH} netstat -a -f ${af} -p ${proto} | fgrep ' *.echo '
.endfor
.endfor
.for ip in ECO_IN ECO_OUT RTT_IN
	ssh ${ECO_SSH} netstat -av -f inet -p udp | fgrep ' ${${ip}}.echo '
	ssh ${ECO_SSH} netstat -av -f inet6 -p udp | fgrep ' ${${ip}6}.echo '
.endfor

.include <bsd.regress.mk>
