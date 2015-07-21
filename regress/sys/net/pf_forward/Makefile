#	$OpenBSD: Makefile,v 1.10 2015/07/21 17:51:50 bluhm Exp $

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
#
# +---+   0   +--+   1   +--+   2   +---+ 3   4 +---+ 5   6 +--+
# |SRC| ----> |PF| ----> |RT| ----> |ECO|       |RDR|       |AF|
# +---+       +--+       +--+       +---+       +---+       +--+
#     out    in  out    in  out    in   out    in   out    in

# Configure Addresses on the machines, there must be routes for the
# networks.  Adapt interface and addresse variables to your local
# setup.  To control the remote machine you need a hostname for
# ssh to log in.
# You must have an anchor "regress" for the divert rules in the pf.conf
# of the PF machine.  The kernel of the PF machine gets testet.
#
# Run make check-setup to see if you got the setup correct.

SRC_IF ?=	tun0
SRC_MAC ?=	fe:e1:ba:d1:0a:dc
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
	echo 'PF_MAC="${PF_MAC}"' >>$@.tmp
.for var in SRC_OUT PF_IN PF_OUT RT_IN RT_OUT ECO_IN ECO_OUT RDR_IN RDR_OUT AF_IN
	echo '${var}="${${var}}"' >>$@.tmp
	echo '${var}6="${${var}6}"' >>$@.tmp
.endfor
	mv $@.tmp $@

# load the pf rules into the kernel of the PF machine
stamp-pfctl: addr.py pf.conf
	cat addr.py ${.CURDIR}/pf.conf | pfctl -n -f -
	cat addr.py ${.CURDIR}/pf.conf | \
	    ssh ${PF_SSH} ${SUDO} pfctl -a regress -f -
	@date >$@

# Set variables so that make runs with and without obj directory.
# Only do that if necessary to keep visible output short.
.if ${.CURDIR} == ${.OBJDIR}
PYTHON =	python2.7 ./
.else
PYTHON =	PYTHONPATH=${.OBJDIR} python2.7 ${.CURDIR}/
.endif

# Ping all addresses.  This ensures that the ip addresses are configured
# and all routing table are set up to allow bidirectional packet flow.
# Note that RDR does not exist physically.  So this traffic is rewritten
# by PF and handled by ECO.
TARGETS +=	ping  ping6

run-regress-ping: stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in SRC_OUT PF_IN PF_OUT RT_IN RT_OUT ECO_IN ECO_OUT RDR_IN RDR_OUT AF_IN
	@echo Check ping ${ip}:
	ping -n -c 1 ${${ip}}
.endfor

run-regress-ping6: stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in SRC_OUT PF_IN PF_OUT RT_IN RT_OUT ECO_IN ECO_OUT RDR_IN RDR_OUT AF_IN
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

run-regress-ping-mtu: addr.py stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN ECO_OUT RDR_IN RDR_OUT
	@echo Check path MTU to ${ip} is 1300
	${SUDO} ${PYTHON}ping_mtu.py ${${ip}} 1300
.endfor
	@echo Check path MTU to AF_IN is 1280
	${SUDO} ${PYTHON}ping_mtu.py ${AF_IN} 1280

run-regress-ping6-mtu: addr.py stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN ECO_OUT RDR_IN RDR_OUT
	@echo Check path MTU to ${ip}6 is 1300
	${SUDO} ${PYTHON}ping6_mtu.py ${${ip}6} 1300
.endfor
	@echo Check path MTU to AF_IN6 is 1320
	${SUDO} ${PYTHON}ping6_mtu.py ${AF_IN6} 1320

# Send one udp echo port 7 packet to all destination addresses with netcat.
# The response must arrive in 1 second.
TARGETS +=	udp  udp6

run-regress-udp: stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN ECO_OUT RDR_IN RDR_OUT AF_IN
	@echo Check udp ${ip}:
	( echo $$$$ | nc -u ${${ip}} 7 & sleep 1; kill $$! ) | grep $$$$
.endfor

run-regress-udp6: stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN ECO_OUT RDR_IN RDR_OUT AF_IN
	@echo Check udp ${ip}6:
	( echo $$$$ | nc -u ${${ip}6} 7 & sleep 1; kill $$! ) | grep $$$$
.endfor

# Send a data stream to tcp echo port 7 to all destination addresses
# with netcat.  Use enough data to make sure PMTU discovery works.
# Count the reflected bytes and compare with the transmitted ones.
TARGETS +=	tcp  tcp6

run-regress-tcp: stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN ECO_OUT RDR_IN RDR_OUT AF_IN
	@echo Check tcp ${ip}:
	openssl rand 200000 | nc -N ${${ip}} 7 | wc -c | grep '200000$$'
.endfor

run-regress-tcp6: stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN ECO_OUT RDR_IN RDR_OUT AF_IN
	@echo Check tcp ${ip}6:
	openssl rand 200000 | nc -N ${${ip}6} 7 | wc -c | grep '200000$$'
.endfor

REGRESS_TARGETS =	${TARGETS:S/^/run-regress-/}

CLEANFILES +=		addr.py *.pyc *.log stamp-*

.PHONY: check-setup

# Check wether the address, route and remote setup is correct
check-setup:
	@echo '\n======== $@ SRC ========'
	ping -n -c 1 ${SRC_OUT}  # SRC_OUT
	route -n get -inet ${SRC_OUT} | fgrep -q 'interface: lo0'  # SRC_OUT
	ping -n -c 1 ${PF_IN}  # PF_IN
	route -n get -inet ${PF_IN} | fgrep -q 'interface: ${SRC_IF}'  # PF_IN SRC_IF
.for ip in PF_OUT RT_IN RT_OUT ECO_IN ECO_OUT RDR_IN RDR_OUT AF_IN
	route -n get -inet ${${ip}} | fgrep -q 'gateway: ${PF_IN}'  # ${ip} PF_IN
.endfor
	ping6 -n -c 1 ${SRC_OUT6}  # SRC_OUT6
	route -n get -inet6 ${SRC_OUT6} | fgrep -q 'interface: lo0'  # SRC_OUT6
	ping6 -n -c 1 ${PF_IN6}  # PF_IN6
	route -n get -inet6 ${PF_IN6} | fgrep -q 'interface: ${SRC_IF}'  # PF_IN6 SRC_IF
.for ip in PF_OUT RT_IN RT_OUT ECO_IN ECO_OUT RDR_IN RDR_OUT AF_IN
	route -n get -inet6 ${${ip}6} | fgrep -q 'gateway: ${PF_IN6}'  # ${ip}6 PF_IN6
.endfor
	@echo '\n======== $@ PF ========'
	ssh ${PF_SSH} ping -n -c 1 ${PF_IN}  # PF_IN
	ssh ${PF_SSH} route -n get -inet ${PF_IN} | fgrep -q 'interface: lo0'  # PF_IN
	ssh ${PF_SSH} ping -n -c 1 ${SRC_OUT}  # SRC_OUT
	ssh ${PF_SSH} ping -n -c 1 ${PF_OUT}  # PF_OUT
	ssh ${PF_SSH} route -n get -inet ${PF_OUT} | fgrep -q 'interface: lo0'  # PF_OUT
	ssh ${PF_SSH} ping -n -c 1 ${RT_IN}  # RT_IN
.for ip in RT_OUT ECO_IN ECO_OUT
	ssh ${PF_SSH} route -n get -inet ${${ip}} | fgrep -q 'gateway: ${RT_IN}'  # ${ip} RT_IN
.endfor
	ssh ${PF_SSH} ping6 -n -c 1 ${PF_IN6}  # PF_IN6
	ssh ${PF_SSH} route -n get -inet6 ${PF_IN6} | fgrep -q 'interface: lo0'  # PF_IN6
	ssh ${PF_SSH} ping6 -n -c 1 ${SRC_OUT6}  # SRC_OUT6
	ssh ${PF_SSH} ping6 -n -c 1 ${PF_OUT6}  # PF_OUT6
	ssh ${PF_SSH} route -n get -inet6 ${PF_OUT6} | fgrep -q 'interface: lo0'  # PF_OUT6
	ssh ${PF_SSH} ping6 -n -c 1 ${RT_IN6}  # RT_IN6
.for ip in RT_OUT ECO_IN ECO_OUT
	ssh ${PF_SSH} route -n get -inet6 ${${ip}6} | fgrep -q 'gateway: ${RT_IN6}'  # ${ip}6 RT_IN6
.endfor
	ssh ${PF_SSH} ${SUDO} pfctl -sr | grep '^anchor "regress" all$$'
	ssh ${PF_SSH} ${SUDO} pfctl -si | grep '^Status: Enabled '
	ssh ${PF_SSH} sysctl net.inet.ip.forwarding | fgrep =1
	ssh ${PF_SSH} sysctl net.inet6.ip6.forwarding | fgrep =1
	@echo '\n======== $@ RT ========'
	ssh ${RT_SSH} ping -n -c 1 ${RT_IN}  # RT_IN
	ssh ${RT_SSH} route -n get -inet ${RT_IN} | fgrep -q 'interface: lo0'  # RT_IN
	ssh ${RT_SSH} ping -n -c 1 ${PF_OUT}  # PF_OUT
.for ip in PF_IN SRC_OUT
	ssh ${RT_SSH} route -n get -inet ${${ip}} | fgrep -q 'gateway: ${PF_OUT}'  # ${ip} PF_OUT
.endfor
	ssh ${RT_SSH} ping -n -c 1 ${RT_OUT}  # RT_OUT
	ssh ${RT_SSH} route -n get -inet ${RT_OUT} | fgrep -q 'interface: lo0'  # RT_OUT
	ssh ${RT_SSH} ping -n -c 1 ${ECO_IN}  # ECO_IN
	ssh ${RT_SSH} route -n get -inet ${ECO_OUT} | fgrep -q 'gateway: ${ECO_IN}'  # ECO_OUT ECO_IN
	ssh ${RT_SSH} ping6 -n -c 1 ${RT_IN6}  # RT_IN6
	ssh ${RT_SSH} route -n get -inet6 ${RT_IN6} | fgrep -q 'interface: lo0'  # RT_IN6
	ssh ${RT_SSH} ping6 -n -c 1 ${PF_OUT6}  # PF_OUT6
.for ip in PF_IN SRC_OUT
	ssh ${RT_SSH} route -n get -inet6 ${${ip}6} | fgrep -q 'gateway: ${PF_OUT6}'  # ${ip}6 PF_OUT6
.endfor
	ssh ${RT_SSH} ping6 -n -c 1 ${RT_OUT6}  # RT_OUT6
	ssh ${RT_SSH} route -n get -inet6 ${RT_OUT6} | fgrep -q 'interface: lo0'  # RT_OUT6
	ssh ${RT_SSH} ping6 -n -c 1 ${ECO_IN6}  # ECO_IN6
	ssh ${RT_SSH} route -n get -inet6 ${ECO_OUT6} | fgrep -q 'gateway: ${ECO_IN6}'  # ECO_OUT6 ECO_IN6
	ssh ${RT_SSH} sysctl net.inet.ip.forwarding | fgrep =1
	ssh ${RT_SSH} sysctl net.inet6.ip6.forwarding | fgrep =1
	ssh ${RT_SSH} ifconfig | fgrep 'mtu 1300'
	@echo '\n======== $@ ECO ========'
.for ip in ECO_IN ECO_OUT
	ssh ${ECO_SSH} ping -n -c 1 ${${ip}}  # ${ip}
	ssh ${ECO_SSH} route -n get -inet ${${ip}} | fgrep -q 'interface: lo0'  # ${ip}
.endfor
	ssh ${ECO_SSH} ping -n -c 1 ${RT_OUT}  # RT_OUT
.for ip in RT_IN PF_OUT PF_IN SRC_OUT
	ssh ${ECO_SSH} route -n get -inet ${${ip}} | fgrep -q 'gateway: ${RT_OUT}'  # ${ip} RT_OUT
.endfor
.for ip in ECO_IN ECO_OUT
	ssh ${ECO_SSH} ping6 -n -c 1 ${${ip}6}  # ${ip}6
	ssh ${ECO_SSH} route -n get -inet6 ${${ip}6} | fgrep -q 'interface: lo0'  # ${ip}6
.endfor
	ssh ${ECO_SSH} ping6 -n -c 1 ${RT_OUT6}  # RT_OUT6
.for ip in RT_IN PF_OUT PF_IN SRC_OUT
	ssh ${ECO_SSH} route -n get -inet6 ${${ip}6} | fgrep -q 'gateway: ${RT_OUT6}'  # ${ip}6 RT_OUT6
.endfor
.for af in inet inet6
.for proto in udp tcp
	ssh ${ECO_SSH} netstat -a -f ${af} -p ${proto} | fgrep ' *.echo '
.endfor
.endfor
	ssh ${ECO_SSH} netstat -av -f inet -p udp | fgrep ' ${ECO_IN}.echo '
	ssh ${ECO_SSH} netstat -av -f inet -p udp | fgrep ' ${ECO_OUT}.echo '
	ssh ${ECO_SSH} netstat -av -f inet6 -p udp | fgrep ' ${ECO_IN6}.echo '
	ssh ${ECO_SSH} netstat -av -f inet6 -p udp | fgrep ' ${ECO_OUT6}.echo '

.include <bsd.regress.mk>
