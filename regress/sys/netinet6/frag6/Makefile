#	$OpenBSD: Makefile,v 1.8 2013/07/23 22:38:45 bluhm Exp $

# The following ports must be installed:
#
# python-2.7          interpreted object-oriented programming language
# py-libdnet          python interface to libdnet
# scapy               powerful interactive packet manipulation in python

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
	${SUDO} python2.7 frag6.py

# An hop by hop options extension header before the fragment header
TARGETS +=	frag6-ext
run-regress-frag6-ext: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 extension header reassembly
	${SUDO} python2.7 frag6_ext.py

# An destination options extension header after the fragment header
TARGETS +=	frag6-opt
run-regress-frag6-opt: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 destination option reassembly
	${SUDO} python2.7 frag6_opt.py

# fragmented packet with head overlapping first fragment
TARGETS +=	frag6-overhead0
run-regress-frag6-overhead0: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 head overlapping first fragment
	${SUDO} python2.7 frag6_overhead0.py

# fragmented packet with head overlapping second fragment
TARGETS +=	frag6-overhead
run-regress-frag6-overhead: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 head overlapping second fragment
	${SUDO} python2.7 frag6_overhead.py

# fragmented packet with tail overlapping last fragment
TARGETS +=	frag6-overtail
run-regress-frag6-overtail: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 tail overlapping last fragment
	${SUDO} python2.7 frag6_overtail.py

# fragmented packet with overlap, drop future fragments
TARGETS +=	frag6-overdrop
run-regress-frag6-overdrop: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 overlap drop future fragments
	${SUDO} python2.7 frag6_overdrop.py

# fragmented packet with overlap, atomic fragment must be processed
TARGETS +=	frag6-overatomic
run-regress-frag6-overatomic: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 overlapping and atomic fragments
	${SUDO} python2.7 frag6_overatomic.py

# atomic fragment with short fragmented payload
TARGETS +=	frag6-shortatomic
run-regress-frag6-shortatomic: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 short atomic fragments
	${SUDO} python2.7 frag6_shortatomic.py

# fragmented packet permuted fragments
TARGETS +=	frag6-permute
run-regress-frag6-permute: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 permuted fragments
	${SUDO} python2.7 frag6_permute.py

# fragmented packet with zero length first fragment
TARGETS +=	frag6-zerofirst
run-regress-frag6-zerofirst: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 zero length first fragment
	${SUDO} python2.7 frag6_zerofirst.py

# fragmented packet with zero length second fragment
TARGETS +=	frag6-zerosecond
run-regress-frag6-zerosecond: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 zero length second fragment
	${SUDO} python2.7 frag6_zerosecond.py

# atomic fragmented udp packet
TARGETS +=	frag6-udpatomic
run-regress-frag6-udpatomic: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 atomic udp fragment
	${SUDO} python2.7 frag6_udpatomic.py

# fragmented udp packet after header
TARGETS +=	frag6-udpheader
run-regress-frag6-udpheader: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 udp header fragment
	${SUDO} python2.7 frag6_udpheader.py

# fragmented udp packet within payload
TARGETS +=	frag6-udppayload
run-regress-frag6-udppayload: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 udp payload fragment
	${SUDO} python2.7 frag6_udppayload.py

# fragmented icmp packet not within 60 second timeout, test takes 90 seconds
TARGETS +=	frag6-timeout
run-regress-frag6-timeout: addr.py
	@echo '\n======== $@ ========'
	@echo Check ping6 fragment timeout
	${SUDO} python2.7 frag6_timeout.py

REGRESS_TARGETS =	${TARGETS:S/^/run-regress-/}

CLEANFILES +=		addr.py *.pyc *.log

.include <bsd.regress.mk>
