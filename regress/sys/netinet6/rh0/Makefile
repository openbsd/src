#	$OpenBSD: Makefile,v 1.2 2013/10/31 01:24:06 bluhm Exp $

# The following ports must be installed:
#
# python-2.7		interpreted object-oriented programming language
# py-libdnet		python interface to libdnet
# scapy			powerful interactive packet manipulation in python

# Check wether all required python packages are installed.  If some
# are missing print a warning and skip the tests, but do not fail.
PYTHON_IMPORT != python2.7 -c 'from scapy.all import *' 2>&1 || true
.if ! empty(PYTHON_IMPORT)
regress:
	@echo '${PYTHON_IMPORT}'
	@echo install python and the scapy module for additional tests
.endif

# This test needs a manual setup of two machines
# Set up machines: SRC DST
# SRC is the machine where this makefile is running.
# DST is running OpenBSD with pf disabled to test the IPv6 stack.
# SRT source routed host, no packets reach this host,
#     it represents just bunch of addresses
#
# +---+   1   +---+       +---+
# |SRC| ----> |DST|       |SRT|
# +---+       +---+       +---+
#     out    in   out    in   out

# Configure Addresses on the machines.
# Adapt interface and address variables to your local setup.
#
SRC_IF ?=
SRC_MAC ?=
DST_MAC ?=

SRC_OUT6 ?=
DST_IN6 ?=
DST_OUT6 ?=
SRT_IN6 ?=
SRT_OUT6 ?=

.if empty (SRC_IF) || empty (SRC_MAC) || empty (DST_MAC) || \
    empty (SRC_OUT6) || empty (DST_IN6) || empty (DST_OUT6) || \
    empty (SRT_IN6) || empty (SRT_OUT6)
regress:
	@echo this tests needs a remote machine to operate on
	@echo SRC_IF SRC_MAC DST_MAC SRC_OUT6 DST_IN6 DST_OUT6
	@echo SRT_IN6 SRT_OUT6 are empty
	@echo fill out these variables for additional tests
.endif

depend: addr.py

# Create python include file containing the addresses.
addr.py: Makefile
	rm -f $@ $@.tmp
	echo 'SRC_IF = "${SRC_IF}"' >>$@.tmp
	echo 'SRC_MAC = "${SRC_MAC}"' >>$@.tmp
	echo 'DST_MAC = "${DST_MAC}"' >>$@.tmp
.for var in SRC_OUT DST_IN DST_OUT SRT_IN SRT_OUT
	echo '${var}6 = "${${var}6}"' >>$@.tmp
.endfor
	mv $@.tmp $@

# Set variables so that make runs with and without obj directory.
# Only do that if necessary to keep visible output short.
.if ${.CURDIR} == ${.OBJDIR}
PYTHON =	python2.7 ./
.else
PYTHON =	PYTHONPATH=${.OBJDIR} python2.7 ${.CURDIR}/
.endif

# Check routes.  This ensures that and the local routing tables is set up.
TARGETS +=	route6
run-regress-route6:
	@echo '\n======== $@ ========'
.for ip in DST_OUT SRT_IN
	@echo Check route6 destination ${ip}6 gateway DST_IN6:
	route -n get -inet6 ${${ip}6}/64 | grep 'gateway: ${DST_IN6}$$'
.endfor

# Ping all addresses.  This ensures that the ip addresses are configured
# and all routing tables are set up to allow bidirectional packet flow.
TARGETS +=	ping6
run-regress-ping6:
	@echo '\n======== $@ ========'
.for ip in SRC_OUT DST_IN DST_OUT
	@echo Check ping6 ${ip}6:
	ping6 -n -c 1 ${${ip}6}
.endfor

# Send ping6 packet with routing header type 0 but empty address list
TARGETS +=	rh0-empty
run-regress-rh0-empty: addr.py
	@echo '\n======== $@ ========'
	@echo Check routing header type 0 with empty address list
	${SUDO} ${PYTHON}rh0_empty.py

TARGETS +=	rh0-final
# Send ping6 packet with routing header type 0 to the final destination
run-regress-rh0-final: addr.py
	@echo '\n======== $@ ========'
	@echo Check routing header type 0 to the final destination
	${SUDO} ${PYTHON}rh0_final.py

TARGETS +=	rh0-route
# Send ping6 packet with routing header type 0 to be source routed
run-regress-rh0-route: addr.py
	@echo '\n======== $@ ========'
	@echo Check routing header type 0 to be source routed
	${SUDO} ${PYTHON}rh0_route.py

REGRESS_TARGETS =	${TARGETS:S/^/run-regress-/}

CLEANFILES +=		addr.py *.pyc *.log

.include <bsd.regress.mk>
