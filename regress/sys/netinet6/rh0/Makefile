#	$OpenBSD: Makefile,v 1.1 2013/10/20 13:45:44 bluhm Exp $

# The following ports must be installed:
#
# python-2.7		interpreted object-oriented programming language
# py-libdnet		python interface to libdnet
# scapy			powerful interactive packet manipulation in python

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
SRC_IF ?=	tun0
SRC_MAC ?=	fe:e1:ba:d1:56:1f
DST_MAC ?=	70:5f:ca:21:8d:70

SRC_OUT6 ?=	fdd7:e83e:66bc:7:fce1:baff:fed2:26be
DST_IN6 ?=	fdd7:e83e:66bc:7:725f:caff:fe21:8d70
DST_OUT6 ?=	fdd7:e83e:66bc:70:725f:caff:fe21:8d70
SRT_IN6 ?=	fdd7:e83e:66bc:70::1
SRT_OUT6 ?=	fdd7:e83e:66bc:71::1

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

# Ping all addresses.  This ensures that the ip addresses are configured
# and all routing tables are set up to allow bidirectional packet flow.
TARGETS +=	ping6
run-regress-ping6:
	@echo '\n======== $@ ========'
.for ip in SRC_OUT DST_IN DST_OUT
	@echo Check ping6 ${ip}6:
	ping6 -n -c 1 ${${ip}6}
.endfor

.for i in empty final route
# Send hand-crafted routing header type 0 packets
TARGETS +=	rh0-${i}
run-regress-rh0-${i}: addr.py
	@echo '\n======== $@ ========'
	@echo Check routing header type 0 ${i}
	${SUDO} python2.7 rh0_${i}.py
.endfor

REGRESS_TARGETS =	${TARGETS:S/^/run-regress-/}

CLEANFILES +=		addr.py *.pyc *.log

.include <bsd.regress.mk>
