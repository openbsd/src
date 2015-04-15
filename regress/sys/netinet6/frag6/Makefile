#	$OpenBSD: Makefile,v 1.11 2015/04/15 21:29:15 bluhm Exp $

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
# DST is running OpenBSD with pf disabled to test the IPv6 stack.
# Enable echo udp6 in inetd.conf of DST to test UDP fragments.
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

# Ping all addresses.  This ensures that the ip addresses are configured
# and all routing table are set up to allow bidirectional packet flow.
TARGETS +=	ping6
run-regress-ping6:
	@echo '\n======== $@ ========'
.for ip in SRC_OUT DST_IN
	@echo Check ping6 ${ip}6:
	ping6 -n -c 1 ${${ip}6}
.endfor

# Ping all addresses again but with 5000 bytes payload.  These large
# packets get fragmented by SRC and must be handled by DST.
TARGETS +=	fragping6
run-regress-fragping6:
	@echo '\n======== $@ ========'
.for ip in SRC_OUT DST_IN
	@echo Check ping6 ${ip}6:
	ping6 -n -c 1 -s 5000 -m ${${ip}6}
.endfor

# Send hand-crafted fragmented packet
TARGETS +=	frag6
run-regress-frag6: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 reassembly
	${SUDO} ${PYTHON}frag6.py

# An hop by hop options extension header before the fragment header
TARGETS +=	frag6-ext
run-regress-frag6-ext: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 extension header reassembly
	${SUDO} ${PYTHON}frag6_ext.py

# An destination options extension header after the fragment header
TARGETS +=	frag6-opt
run-regress-frag6-opt: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 destination option reassembly
	${SUDO} ${PYTHON}frag6_opt.py

# The ethernet frame has some padding that must be ignored by reassembly
TARGETS +=	frag6-padding
run-regress-frag6-padding: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 ethernet padding
	${SUDO} ${PYTHON}frag6_padding.py

# fragmented packet with head overlapping first fragment
TARGETS +=	frag6-overhead0
run-regress-frag6-overhead0: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 head overlapping first fragment
	${SUDO} ${PYTHON}frag6_overhead0.py

# fragmented packet with head overlapping second fragment
TARGETS +=	frag6-overhead
run-regress-frag6-overhead: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 head overlapping second fragment
	${SUDO} ${PYTHON}frag6_overhead.py

# fragmented packet with tail overlapping last fragment
TARGETS +=	frag6-overtail
run-regress-frag6-overtail: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 tail overlapping last fragment
	${SUDO} ${PYTHON}frag6_overtail.py

# fragmented packet with overlap, drop future fragments
TARGETS +=	frag6-overdrop
run-regress-frag6-overdrop: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 overlap drop future fragments
	${SUDO} ${PYTHON}frag6_overdrop.py

# fragmented packet with overlap, atomic fragment must be processed
TARGETS +=	frag6-overatomic
run-regress-frag6-overatomic: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 overlapping and atomic fragments
	${SUDO} ${PYTHON}frag6_overatomic.py

# atomic fragment with short fragmented payload
TARGETS +=	frag6-shortatomic
run-regress-frag6-shortatomic: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 short atomic fragments
	${SUDO} ${PYTHON}frag6_shortatomic.py

# fragmented packet permuted fragments
TARGETS +=	frag6-permute
run-regress-frag6-permute: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 permuted fragments
	${SUDO} ${PYTHON}frag6_permute.py

# fragmented packet with zero length first fragment
TARGETS +=	frag6-zerofirst
run-regress-frag6-zerofirst: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 zero length first fragment
	${SUDO} ${PYTHON}frag6_zerofirst.py

# fragmented packet with zero length second fragment
TARGETS +=	frag6-zerosecond
run-regress-frag6-zerosecond: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 zero length second fragment
	${SUDO} ${PYTHON}frag6_zerosecond.py

# fragmented large ping packet that has to be refragmented by reflector
TARGETS +=	frag6-refrag
run-regress-frag6-refrag: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 with fragmented reply
	${SUDO} ${PYTHON}frag6_refrag.py

# atomic fragmented udp packet
TARGETS +=	frag6-udpatomic
run-regress-frag6-udpatomic: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 atomic udp fragment
	${SUDO} ${PYTHON}frag6_udpatomic.py

# fragmented udp packet after header
TARGETS +=	frag6-udpheader
run-regress-frag6-udpheader: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 udp header fragment
	${SUDO} ${PYTHON}frag6_udpheader.py

# fragmented udp packet within payload
TARGETS +=	frag6-udppayload
run-regress-frag6-udppayload: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 udp payload fragment
	${SUDO} ${PYTHON}frag6_udppayload.py

# fragmented icmp packet not within 60 second timeout, test takes 90 seconds
TARGETS +=	frag6-timeout
run-regress-frag6-timeout: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 fragment timeout
	${SUDO} ${PYTHON}frag6_timeout.py

REGRESS_TARGETS =	${TARGETS:S/^/run-regress-/}

CLEANFILES +=		addr.py *.pyc *.log

.include <bsd.regress.mk>
