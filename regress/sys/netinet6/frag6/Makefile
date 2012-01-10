#	$OpenBSD: Makefile,v 1.2 2012/01/10 17:21:52 bluhm Exp $

# The following ports must be installed:
#
# python-2.7          interpreted object-oriented programming language
# py-libdnet          python interface to libdnet
# scapy               powerful interactive packet manipulation in python

# This test needs a manual setup of two machines
# Set up machines: SRC DST
# SRC is the machine where this makefile is running.
# DST is running OpenBSD with pf disabled to test the IPv6 stack.
#
# +---+   1   +---+
# |SRC| ----> |DST|
# +---+       +---+
#     out    in

# Configure Addresses on the machines.
# Adapt interface and addresse variables to your local setup.
#
SRC_IF =	tun0
SRC_MAC =	fe:e1:ba:d1:56:1f
DST_MAC =	52:54:00:12:34:50

SRC_OUT6 =	fdd7:e83e:66bc:211:fce1:baff:fed1:561f
DST_IN6 =	fdd7:e83e:66bc:211:5054:ff:fe12:3450

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
	${SUDO} python2.7 frag6.py ${DST_IN6}

# An hop by hop options extension header before the fragment header
TARGETS +=	frag6-ext
run-regress-frag6-ext: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 extension header reassembly
	${SUDO} python2.7 frag6_ext.py ${DST_IN6}

# fragmented packet with head overlapping first fragment
TARGETS +=	frag6-overhead0
run-regress-frag6-overhead0: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 head overlapping first fragment
	${SUDO} python2.7 frag6_overhead0.py ${DST_IN6}

# fragmented packet with head overlapping second fragment
TARGETS +=	frag6-overhead
run-regress-frag6-overhead: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 head overlapping second fragment
	${SUDO} python2.7 frag6_overhead.py ${DST_IN6}

# fragmented packet with tail overlapping last fragment
TARGETS +=	frag6-overtail
run-regress-frag6-overtail: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 tail overlapping last fragment
	${SUDO} python2.7 frag6_overtail.py ${DST_IN6}

# fragmented packet with overlap, drop future fragments
TARGETS +=	frag6-overdrop
run-regress-frag6-overdrop: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 overlap drop future fragments
	${SUDO} python2.7 frag6_overdrop.py ${DST_IN6}

# fragmented packet permuted fragments
TARGETS +=	frag6-permute
run-regress-frag6-permute: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 permuted fragments
	${SUDO} python2.7 frag6_permute.py ${DST_IN6}

REGRESS_TARGETS =	${TARGETS:S/^/run-regress-/}

CLEANFILES +=		*.pyc *.log

.include <bsd.regress.mk>
