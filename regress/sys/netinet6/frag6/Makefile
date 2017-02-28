#	$OpenBSD: Makefile,v 1.16 2017/02/28 16:08:10 bluhm Exp $

# The following ports must be installed:
#
# python-2.7          interpreted object-oriented programming language
# py-libdnet          python interface to libdnet
# scapy               powerful interactive packet manipulation in python

.if ! (make(clean) || make(cleandir) || make(obj))
# Check wether all required python packages are installed.  If some
# are missing print a warning and skip the tests, but do not fail.
PYTHON_IMPORT !!= python2.7 -c 'from scapy.all import *' 2>&1 || true
.endif

.if ! empty(PYTHON_IMPORT)
regress:
	@echo '${PYTHON_IMPORT}'
	@echo Install python and the scapy module for additional tests.
	@echo SKIPPED
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
    empty (SRC_OUT6) || empty (DST_IN6) || empty (REMOTE_SSH)
regress:
	@echo This tests needs a remote machine to operate on.
	@echo SRC_IF SRC_MAC DST_MAC SRC_OUT6 DST_IN6 REMOTE_SSH are empty.
	@echo Fill out these variables for additional tests.
	@echo SKIPPED
.endif

.MAIN: all

.if make (regress) || make (all)
.BEGIN: addr.py
	@echo
	${SUDO} true
	rm -f stamp-stack stamp-pf
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

stamp-stack:
	rm -f stamp-stack stamp-pf
	-ssh -t ${REMOTE_SSH} ${SUDO} pfctl -d
	ssh -t ${REMOTE_SSH} ${SUDO} pfctl -a regress -Fr
	date >$@

stamp-pf:
	rm -f stamp-stack stamp-pf
	echo 'pass proto tcp from port ssh no state\n'\
	    'pass proto tcp to port ssh no state'|\
	    ssh -t ${REMOTE_SSH} ${SUDO} pfctl -a regress -f -
	-ssh -t ${REMOTE_SSH} ${SUDO} pfctl -e
	date >$@

REGRESS_TARGETS =
FRAG6_SCRIPTS !!=	cd ${.CURDIR} && ls -1 frag6*.py

.for sp in stack pf

# Ping all addresses.  This ensures that the ip addresses are configured
# and all routing table are set up to allow bidirectional packet flow.
${sp}: run-regress-${sp}-ping6
run-regress-${sp}-ping6: stamp-${sp}
	@echo '\n======== $@ ========'
.for ip in SRC_OUT DST_IN
	@echo Check ping6 ${ip}6:
	ping6 -n -c 1 ${${ip}6}
.endfor

# Ping all addresses again but with 5000 bytes payload.  These large
# packets get fragmented by SRC and must be handled by DST.
${sp}: run-regress-${sp}-fragping6
run-regress-${sp}-fragping6: stamp-${sp}
	@echo '\n======== $@ ========'
.for ip in SRC_OUT DST_IN
	@echo Check ping6 ${ip}6:
	ping6 -n -c 1 -s 5000 -m ${${ip}6}
.endfor

.for s in ${FRAG6_SCRIPTS}
${sp}: run-regress-${sp}-${s}
run-regress-${sp}-${s}: addr.py stamp-${sp}
	@echo '\n======== $@ ========'
	${SUDO} ${PYTHON}${s}
.endfor

REGRESS_TARGETS +=	run-regress-${sp}-ping6 run-regress-${sp}-fragping6 \
			${FRAG6_SCRIPTS:S/^/run-regress-${sp}-/}

.endfor

# After running the tests, turn on pf on remote machine.
# This is the expected default configuration.
REGRESS_TARGETS +=	stamp-pf

CLEANFILES +=		addr.py *.pyc *.log stamp-*

.include <bsd.regress.mk>
