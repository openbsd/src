#	$OpenBSD: Makefile,v 1.2 2016/07/18 09:09:24 bluhm Exp $

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

# This test needs a manual setup of two machines
# Set up machines: LOCAL REMOTE
# LOCAL is the machine where this makefile is running.
# REMOTE is running OpenBSD with echo and chargen server to test PMTU
# FAKE is an non existing machine in a non existing network.
# REMOTE_SSH is the hostname to log in on the REMOTE machine.

# Configure Addresses on the machines.
# Adapt interface and addresse variables to your local setup.
#
LOCAL_IF ?=
REMOTE_SSH ?=

LOCAL_ADDR ?=
REMOTE_ADDR ?=
FAKE_NET ?=
FAKE_NET_ADDR ?=

LOCAL_ADDR6 ?=
REMOTE_ADDR6 ?=
FAKE_NET6 ?=
FAKE_NET_ADDR6 ?=

.if empty (LOCAL_IF) || empty (REMOTE_SSH) || \
    empty (LOCAL_ADDR) || empty (LOCAL_ADDR6) || \
    empty (REMOTE_ADDR) || empty (REMOTE_ADDR6) || \
    empty (FAKE_NET) || empty (FAKE_NET6) || \
    empty (FAKE_NET_ADDR) || empty (FAKE_NET_ADDR6)
regress:
	@echo This tests needs a remote machine to operate on
	@echo LOCAL_IF REMOTE_SSH LOCAL_ADDR LOCAL_ADDR6 REMOTE_ADDR
	@echo REMOTE_ADDR6 FAKE_NET FAKE_NET6 FAKE_NET_ADDR FAKE_NET_ADDR6
	@echo are empty.  Fill out these variables for additional tests.
.endif

.if ! empty (REMOTE_SSH)
.if make (regress) || make (all)
.BEGIN: pf.conf addr.py
	@echo
	${SUDO} true
	ssh -t ${REMOTE_SSH} ${SUDO} true
.endif
.endif

depend: addr.py

# Create python include file containing the addresses.
addr.py: Makefile
	rm -f $@ $@.tmp
	echo 'LOCAL_IF = "${LOCAL_IF}"' >>$@.tmp
	echo 'LOCAL_MAC = "${LOCAL_MAC}"' >>$@.tmp
	echo 'REMOTE_MAC = "${REMOTE_MAC}"' >>$@.tmp
.for var in LOCAL REMOTE FAKE_NET
	echo '${var}_ADDR = "${${var}_ADDR}"' >>$@.tmp
	echo '${var}_ADDR6 = "${${var}_ADDR6}"' >>$@.tmp
.endfor
	echo 'FAKE_NET = "${FAKE_NET}"' >>$@.tmp
	echo 'FAKE_NET6 = "${FAKE_NET6}"' >>$@.tmp
	mv $@.tmp $@

# Set variables so that make runs with and without obj directory.
# Only do that if necessary to keep visible output short.
.if ${.CURDIR} == ${.OBJDIR}
PYTHON =	python2.7 -u ./
.else
PYTHON =	PYTHONPATH=${.OBJDIR} python2.7 -u ${.CURDIR}/
.endif

.PHONY: clean-arp

# Clear local and remote path mtu routes, set fake net route
reset-route:
	@echo '\n======== $@ ========'
	-${SUDO} route -n delete -host ${REMOTE_ADDR}
	ssh ${REMOTE_SSH} ${SUDO} sh -c "'\
	    route -n delete -inet -host ${LOCAL_ADDR};\
	    route -n delete -inet -net ${FAKE_NET};\
	    route -n delete -inet -host ${FAKE_NET_ADDR};\
	    route -n add -inet -net ${FAKE_NET} ${LOCAL_ADDR}'"
reset-route6:
	@echo '\n======== $@ ========'
	-${SUDO} route -n delete -host ${REMOTE_ADDR6}
	ssh ${REMOTE_SSH} ${SUDO} sh -c "'\
	    route -n delete -inet6 -host ${LOCAL_ADDR6};\
	    route -n delete -inet6 -net ${FAKE_NET6};\
	    route -n delete -inet6 -host ${FAKE_NET_ADDR6};\
	    route -n add -inet6 -net ${FAKE_NET6} ${LOCAL_ADDR6}'"

# Clear host routes and ping all addresses.  This ensures that
# the IP addresses are configured and all routing table are set up
# to allow bidirectional packet flow.
TARGETS +=	ping ping6
run-regress-ping: reset-route
	@echo '\n======== $@ ========'
.for ip in LOCAL_ADDR REMOTE_ADDR
	@echo Check ping ${ip}
	ping -n -c 1 ${${ip}}
.endfor
run-regress-ping6: reset-route
	@echo '\n======== $@ ========'
.for ip in LOCAL_ADDR REMOTE_ADDR
	@echo Check ping6 ${ip}6
	ping6 -n -c 1 ${${ip}6}
.endfor

TARGETS +=	pmtu pmtu6
run-regress-pmtu: addr.py reset-route
	@echo '\n======== $@ ========'
	@echo Send ICMP fragmentation needed after fake TCP connect
	${SUDO} ${PYTHON}tcp_connect.py
run-regress-pmtu6: addr.py reset-route6
	@echo '\n======== $@ ========'
	@echo Send ICMP6 packet too big after fake TCP connect
	${SUDO} ${PYTHON}tcp_connect6.py

TARGETS +=	udp6
run-regress-udp6: addr.py reset-route6
	@echo '\n======== $@ ========'
	@echo Send ICMP6 packet too big after UDP echo
	${SUDO} ${PYTHON}udp_echo6.py

TARGETS +=	gateway6
run-regress-gateway6: run-regress-udp6
	@echo '\n======== $@ ========'
	@echo Remove gateway route of a dynamic PMTU route
	ssh ${REMOTE_SSH} ${SUDO} route -n delete -inet6 -host ${LOCAL_ADDR6}
	ssh ${REMOTE_SSH} route -n get -inet6 -host ${FAKE_NET_ADDR6}\
	    >pmtu.route
	cat pmtu.route
	grep -q 'gateway: ${LOCAL_ADDR6}' pmtu.route
	grep -q 'flags: <UP,GATEWAY,HOST,DYNAMIC,DONE>' pmtu.route
	${SUDO} ${PYTHON}udp_echo6.py

REGRESS_TARGETS =	${TARGETS:S/^/run-regress-/}

CLEANFILES +=		addr.py *.pyc *.log *.route

.include <bsd.regress.mk>
