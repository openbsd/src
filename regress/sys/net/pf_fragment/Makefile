#	$OpenBSD: Makefile,v 1.12 2015/07/20 20:53:44 bluhm Exp $

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
# Set up machines: SRC PF RT ECO
# SRC is the machine where this makefile is running.
# PF is running OpenBSD forwarding through pf, it is the test target.
# RT is a router forwarding packets, maximum MTU is 1300.
# ECO is reflecting the ping and UDP echo packets.
# RDR does not exist, PF redirects the traffic to ECO.
#
# +---+   0   +--+   1   +--+   2   +---+ 3   4 +---+
# |SRC| ----> |PF| ----> |RT| ----> |ECO|       |RDR|
# +---+       +--+       +--+       +---+       +---+
#     out    in  out    in  out    in          in

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
RDR_IN ?=	10.188.214.188

SRC_OUT6 ?=	fdd7:e83e:66bc:210:fce1:baff:fed1:561f
PF_IN6 ?=	fdd7:e83e:66bc:210:5054:ff:fe12:3450
PF_OUT6 ?=	fdd7:e83e:66bc:211:5054:ff:fe12:3450
RT_IN6 ?=	fdd7:e83e:66bc:211:5054:ff:fe12:3451
RT_OUT6 ?=	fdd7:e83e:66bc:212:5054:ff:fe12:3451
ECO_IN6 ?=	fdd7:e83e:66bc:212:5054:ff:fe12:3452
RDR_IN6 ?=	fdd7:e83e:66bc:214::188

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
.for var in SRC_OUT PF_IN PF_OUT RT_IN RT_OUT ECO_IN RDR_IN
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
.for ip in SRC_OUT PF_IN PF_OUT RT_IN RT_OUT ECO_IN RDR_IN
	@echo Check ping ${ip}:
	ping -n -c 1 ${${ip}}
.endfor

run-regress-ping6: stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in SRC_OUT PF_IN PF_OUT RT_IN RT_OUT ECO_IN RDR_IN
	@echo Check ping ${ip}6:
	ping6 -n -c 1 ${${ip}6}
.endfor

# Ping all addresses again but with 5000 bytes payload.  These large
# packets get fragmented by SRC and must be handled by PF.
# Send 1 packet in advance for Path-MTU discovery.
TARGETS +=	fragping  fragping6

run-regress-fragping: stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in PF_IN PF_OUT RT_IN RT_OUT ECO_IN RDR_IN
	@echo Check ping ${ip}:
	-ping -n -c 1 -s 1400 -D ${${ip}}
	ping -n -c 1 -s 5000 ${${ip}}
.endfor

run-regress-fragping6: stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in PF_IN PF_OUT RT_IN RT_OUT ECO_IN RDR_IN
	@echo Check ping ${ip}6:
	-ping6 -n -c 1 -s 1400 -m ${${ip}6}
	ping6 -n -c 1 -s 5000 -m ${${ip}6}
.endfor

# Send a large IPv4/ICMP-Echo-Request packet with enabled DF bit and
# parse response packet to determine MTU of the router.  The MTU has
# to be 1300 octets.  The MTU has to be defined at out interface of
# the router RT before.
TARGETS +=	ping-mtu

# Currently this test fails as pf does not fix the checksum of
# NATed packets inside of icmp packets.
# XXX TARGETS +=	ping6-mtu

run-regress-ping-mtu: addr.py stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN RDR_IN
	@echo Check path MTU to ${ip}
	${SUDO} ${PYTHON}ping_mtu_1300.py ${${ip}}
.endfor

run-regress-ping6-mtu: addr.py stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN RDR_IN
	@echo Check path MTU to ${ip}6
	${SUDO} ${PYTHON}ping6_mtu_1300.py ${${ip}6}
.endfor

# Send packet to big to get to destination.
# Check that checksum of the quoted original packet in icmp is correct.

# Currently these test fail as pf does not fix the checksum of
# NATed packets inside of icmp packets.
# XXX TARGETS +=	ping-cksum ping6-cksum udp-cksum udp6-cksum

run-regress-ping-cksum: addr.py stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN RDR_IN
	@echo Check icmp chksum in fragmentation needed to ${ip}
	${SUDO} ${PYTHON}ping_cksum.py ${${ip}}
.endfor

run-regress-ping6-cksum: addr.py stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN RDR_IN
	@echo Check icmp6 chksum in packet too big to ${ip}6
	${SUDO} ${PYTHON}ping6_cksum.py ${${ip}6}
.endfor

run-regress-udp-cksum: addr.py stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN RDR_IN
	@echo Check udp chksum in packet too big to ${ip}
	${SUDO} ${PYTHON}udp_cksum.py ${${ip}}
.endfor

run-regress-udp6-cksum: addr.py stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN RDR_IN
	@echo Check udp6 chksum in packet too big to ${ip}6
	${SUDO} ${PYTHON}udp6_cksum.py ${${ip}6}
.endfor

# Send handcrafted fragmented packets with overlaps
TARGETS +=	frag frag6

run-regress-frag: addr.py stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN RDR_IN
	@echo Check ping reassembly ${ip}
	${SUDO} ${PYTHON}frag.py ${${ip}}
.endfor

run-regress-frag6: addr.py stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN RDR_IN
	@echo Check ping6 reassembly ${ip}6
	${SUDO} ${PYTHON}frag6.py ${${ip}6}
.endfor

TARGETS +=	frag6-ext

run-regress-frag6-ext: addr.py stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN RDR_IN
	@echo Check ping6 extension header reassembly ${ip}6
	${SUDO} ${PYTHON}frag6_ext.py ${${ip}6}
.endfor

TARGETS +=	frag-cutnew frag6-cutnew

run-regress-frag-cutnew: addr.py stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN RDR_IN
	@echo Check ping head overlap reassembly ${ip}
	${SUDO} ${PYTHON}frag_cutnew.py ${${ip}}
.endfor

run-regress-frag6-cutnew: addr.py stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN RDR_IN
	@echo Check ping6 head overlap reassembly ${ip}6
	${SUDO} ${PYTHON}frag6_cutnew.py ${${ip}6}
.endfor

TARGETS +=	frag-cutold frag6-cutold

run-regress-frag-cutold: addr.py stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN RDR_IN
	@echo Check ping tail overlap reassembly ${ip}
	${SUDO} ${PYTHON}frag_cutold.py ${${ip}}
.endfor

run-regress-frag6-cutold: addr.py stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN RDR_IN
	@echo Check ping6 tail overlap reassembly ${ip}6
	${SUDO} ${PYTHON}frag6_cutold.py ${${ip}6}
.endfor

TARGETS +=	frag-dropold frag6-dropold

run-regress-frag-dropold: addr.py stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN RDR_IN
	@echo Check ping total overlapping reassembly ${ip}
	${SUDO} ${PYTHON}frag_dropold.py ${${ip}}
.endfor

run-regress-frag6-dropold: addr.py stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN RDR_IN
	@echo Check ping6 total overlapping reassembly ${ip}6
	${SUDO} ${PYTHON}frag6_dropold.py ${${ip}6}
.endfor

TARGETS +=	frag-dropnew frag6-dropnew

run-regress-frag-dropnew: addr.py stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN RDR_IN
	@echo Check ping total overlaped reassembly ${ip}
	${SUDO} ${PYTHON}frag_dropnew.py ${${ip}}
.endfor

run-regress-frag6-dropnew: addr.py stamp-pfctl
	@echo '\n======== $@ ========'
.for ip in ECO_IN RDR_IN
	@echo Check ping6 total overlaped reassembly ${ip}6
	${SUDO} ${PYTHON}frag6_dropnew.py ${${ip}6}
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
.for ip in PF_OUT RT_IN RT_OUT ECO_IN RDR_IN
	route -n get -inet ${${ip}} | fgrep -q 'gateway: ${PF_IN}'  # ${ip} PF_IN
.endfor
	ping6 -n -c 1 ${SRC_OUT6}  # SRC_OUT6
	route -n get -inet6 ${SRC_OUT6} | fgrep -q 'interface: lo0'  # SRC_OUT6
	ping6 -n -c 1 ${PF_IN6}  # PF_IN6
	route -n get -inet6 ${PF_IN6} | fgrep -q 'interface: ${SRC_IF}'  # PF_IN6 SRC_IF
.for ip in PF_OUT RT_IN RT_OUT ECO_IN RDR_IN
	route -n get -inet6 ${${ip}6} | fgrep -q 'gateway: ${PF_IN6}'  # ${ip}6 PF_IN6
.endfor
	@echo '\n======== $@ PF ========'
	ssh ${PF_SSH} ping -n -c 1 ${PF_IN}  # PF_IN
	ssh ${PF_SSH} route -n get -inet ${PF_IN} | fgrep -q 'interface: lo0'  # PF_IN
	ssh ${PF_SSH} ping -n -c 1 ${SRC_OUT}  # SRC_OUT
	ssh ${PF_SSH} ping -n -c 1 ${PF_OUT}  # PF_OUT
	ssh ${PF_SSH} route -n get -inet ${PF_OUT} | fgrep -q 'interface: lo0'  # PF_OUT
	ssh ${PF_SSH} ping -n -c 1 ${RT_IN}  # RT_IN
.for ip in RT_OUT ECO_IN
	ssh ${PF_SSH} route -n get -inet ${${ip}} | fgrep -q 'gateway: ${RT_IN}'  # ${ip} RT_IN
.endfor
	ssh ${PF_SSH} ping6 -n -c 1 ${PF_IN6}  # PF_IN6
	ssh ${PF_SSH} route -n get -inet6 ${PF_IN6} | fgrep -q 'interface: lo0'  # PF_IN6
	ssh ${PF_SSH} ping6 -n -c 1 ${SRC_OUT6}  # SRC_OUT6
	ssh ${PF_SSH} ping6 -n -c 1 ${PF_OUT6}  # PF_OUT6
	ssh ${PF_SSH} route -n get -inet6 ${PF_OUT6} | fgrep -q 'interface: lo0'  # PF_OUT6
	ssh ${PF_SSH} ping6 -n -c 1 ${RT_IN6}  # RT_IN6
.for ip in RT_OUT ECO_IN
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
	ssh ${RT_SSH} ping6 -n -c 1 ${RT_IN6}  # RT_IN6
	ssh ${RT_SSH} route -n get -inet6 ${RT_IN6} | fgrep -q 'interface: lo0'  # RT_IN6
	ssh ${RT_SSH} ping6 -n -c 1 ${PF_OUT6}  # PF_OUT6
.for ip in PF_IN SRC_OUT
	ssh ${RT_SSH} route -n get -inet6 ${${ip}6} | fgrep -q 'gateway: ${PF_OUT6}'  # ${ip}6 PF_OUT6
.endfor
	ssh ${RT_SSH} ping6 -n -c 1 ${RT_OUT6}  # RT_OUT6
	ssh ${RT_SSH} route -n get -inet6 ${RT_OUT6} | fgrep -q 'interface: lo0'  # RT_OUT6
	ssh ${RT_SSH} ping6 -n -c 1 ${ECO_IN6}  # ECO_IN6
	ssh ${RT_SSH} sysctl net.inet.ip.forwarding | fgrep =1
	ssh ${RT_SSH} sysctl net.inet6.ip6.forwarding | fgrep =1
	ssh ${RT_SSH} ifconfig | fgrep 'mtu 1300'
	@echo '\n======== $@ ECO ========'
	ssh ${ECO_SSH} ping -n -c 1 ${ECO_IN}  # ECO_IN
	ssh ${ECO_SSH} route -n get -inet ${ECO_IN} | fgrep -q 'interface: lo0'  # ECO_IN
	ssh ${ECO_SSH} ping -n -c 1 ${RT_OUT}  # RT_OUT
.for ip in RT_IN PF_OUT PF_IN SRC_OUT
	ssh ${ECO_SSH} route -n get -inet ${${ip}} | fgrep -q 'gateway: ${RT_OUT}'  # ${ip} RT_OUT
.endfor
	ssh ${ECO_SSH} ping6 -n -c 1 ${ECO_IN6}  # ECO_IN6
	ssh ${ECO_SSH} route -n get -inet6 ${ECO_IN6} | fgrep -q 'interface: lo0'  # ECO_IN6
	ssh ${ECO_SSH} ping6 -n -c 1 ${RT_OUT6}  # RT_OUT6
.for ip in RT_IN PF_OUT PF_IN SRC_OUT
	ssh ${ECO_SSH} route -n get -inet6 ${${ip}6} | fgrep -q 'gateway: ${RT_OUT6}'  # ${ip}6 RT_OUT6
.endfor

.include <bsd.regress.mk>
