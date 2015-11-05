#	$OpenBSD: Makefile,v 1.3 2015/11/05 12:56:12 bluhm Exp $

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
# Set up machines: SRC DST
# SRC is the machine where this makefile is running.
# DST is running OpenBSD with pf to test the neighbor discovery states.
#
# +---+   1   +---+
# |SRC| ----> |DST|
# +---+       +---+
#     out    in

# Configure Addresses on the machines.
# Adapt interface and addresse variables to your local setup.
#
SRC_IF ?=
SRC_MAC ?=
DST_MAC ?=

SRC_OUT6 ?=
DST_IN6 ?=

# pf rules on DST should look like this:
#
# block log
# pass inet6 proto icmp6 icmp6-type echoreq keep state
# pass inet6 proto icmp6 icmp6-type neighbrsol keep state
# pass inet6 proto icmp6 icmp6-type neighbradv keep state

# RFC 4861 7. describes the following test cases for ND:
#
# Duplicate Address Detection
# - request  NS from unspecified address to target solicitated-node multicast
# - response NA from interface address   to all-nodes multicast
#
# Address Resolution
# - request  NS from interface address   to target solicitated-node multicast
# - response NA from interface address   to source of NS
#
# Unsolicited Neighbor Advertisements
# - request  NA from interface address   to all-nodes multicast
#
# Neighbor Unreachability Detection
# - request  NS from interface address   to target unicast
# - response NA from interface address   to source of NS

.if empty (SRC_IF) || empty (SRC_MAC) || empty (DST_MAC) || \
    empty (SRC_OUT6) || empty (DST_IN6)
regress:
	@echo this tests needs a remote machine to operate on
	@echo SRC_IF SRC_MAC DST_MAC SRC_OUT6 DST_IN6 are empty
	@echo fill out these variables for additional tests
.endif

depend: addr.py

# Create python include file containing the addresses.
addr.py: Makefile
	rm -f $@ $@.tmp
	echo 'SRC_IF = "${SRC_IF}"' >>$@.tmp
	echo 'SRC_MAC = "${SRC_MAC}"' >>$@.tmp
	echo 'DST_MAC = "${DST_MAC}"' >>$@.tmp
.for var in SRC_OUT DST_IN
	echo '${var} = "${${var}}"' >>$@.tmp
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

# Clear neighbor cache and ping all addresses.  This ensures that
# the ip addresses are configured and all routing table are set up
# to allow bidirectional packet flow.
TARGETS +=	ping6
run-regress-ping6:
	@echo '\n======== $@ ========'
	${SUDO} ndp -c
.for ip in SRC_OUT DST_IN
	@echo Check ping6 ${ip}6:
	ping6 -n -c 1 ${${ip}6}
.endfor

# Send hand-crafted duplicate address detection neighbor solicitation packet
TARGETS +=	nd6_dad
run-regress-nd6_dad: addr.py
	@echo '\n======== $@ ========'
	@echo Check duplicate address detection
	${SUDO} ${PYTHON}nd6_dad.py

# Send hand-crafted address resolution neighbor solicitation packet
TARGETS +=	nd6_ar
run-regress-nd6_ar: addr.py
	@echo '\n======== $@ ========'
	@echo Check address resolution
	${SUDO} ${PYTHON}nd6_ar.py

# Send hand-crafted unsolicited neighbor advertisement packet
TARGETS +=	nd6_una
run-regress-nd6_una: addr.py
	@echo '\n======== $@ ========'
	@echo Check unsolicited neighbor advertisement
	${SUDO} ${PYTHON}nd6_una.py

# Send hand-crafted neighbor unreachability detection solicitation packet
TARGETS +=	nd6_nud
run-regress-nd6_nud: addr.py
	@echo '\n======== $@ ========'
	@echo Check neighbor unreachability detection
	${SUDO} ${PYTHON}nd6_nud.py

REGRESS_TARGETS =	${TARGETS:S/^/run-regress-/}

CLEANFILES +=		addr.py *.pyc *.log

.include <bsd.regress.mk>
