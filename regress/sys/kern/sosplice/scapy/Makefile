#	$OpenBSD: Makefile,v 1.3 2017/11/05 13:15:36 bluhm Exp $

# The following ports must be installed:
#
# python-2.7          interpreted object-oriented programming language
# py-libdnet          python interface to libdnet
# scapy               powerful interactive packet manipulation in python

.include <bsd.own.mk>

.if ! (make(clean) || make(cleandir) || make(obj))
# Check wether all required python packages are installed.  If some
# are missing print a warning and skip the tests, but do not fail.
PYTHON_IMPORT !!= python2.7 -c 'from scapy.all import *' 2>&1 || true
PF_ENABLED !!=	${SUDO} pfctl -si | grep '^Status: Enabled ' || true
PF_ANCHOR !!=	${SUDO} pfctl -sr | grep '^anchor "regress" all$$' || true
.endif

.if ! empty(PYTHON_IMPORT)
.BEGIN:
	@true
regress:
	@echo '${PYTHON_IMPORT}'
	@echo Install python and the scapy module for additional tests.
	@echo SKIPPED
.endif

.if ! empty (PERL_REQUIRE)
regress:
	@echo "${PERL_REQUIRE}"
	@echo install these perl packages for additional tests
.endif

.if ! empty(PF_ENABLED) && empty(PF_ANCHOR)
.BEGIN:
	@true
regress:
	@echo This test cannot run with pf filtering packets on localhost.
	@echo Either disable pf or add a line 'anchor "regress"' to pf.conf.
	@echo There the test will add a stateless rule that passes all packets.
	@echo SKIPPED
.endif

# This test needs a manual setup of two machines
# Set up machines: LOCAL REMOTE
# LOCAL is the machine where this makefile is running.
# REMOTE is running OpenBSD with echo server to test PMTU
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

.if empty (LOCAL_IF) || empty (REMOTE_SSH) || \
    empty (LOCAL_ADDR) || \
    empty (REMOTE_ADDR) || \
    empty (FAKE_NET) || \
    empty (FAKE_NET_ADDR)
.BEGIN:
	@true
regress:
	@echo This tests needs a remote machine to operate on
	@echo LOCAL_IF REMOTE_SSH LOCAL_ADDR REMOTE_ADDR FAKE_NET FAKE_NET_ADDR
	@echo are empty.  Fill out these variables for additional tests.
	@echo SKIPPED
.endif

.MAIN: all

.if make (regress) || make (all)
.BEGIN: addr.py
	@echo
	${SUDO} true
	ssh -t ${REMOTE_SSH} true
.endif

# Create python include file containing the addresses.
addr.py: Makefile
	rm -f $@ $@.tmp
	echo 'LOCAL_IF = "${LOCAL_IF}"' >>$@.tmp
.for var in LOCAL REMOTE FAKE_NET
	echo '${var}_ADDR = "${${var}_ADDR}"' >>$@.tmp
.endfor
	echo 'FAKE_NET = "${FAKE_NET}"' >>$@.tmp
	mv $@.tmp $@

# load the pf rules into the kernel of the REMOTE and LOCAL machine
stamp-pfctl:
.if ! empty(PF_ANCHOR)
	echo 'pass on ${LOCAL_IF} no state' | ${SUDO} pfctl -a regress -f -
.endif
	@date >$@

SCAPYS !=		cd ${.CURDIR} && ls scapy-*.py
TARGETS =		${SCAPYS}

# Set variables so that make runs with and without obj directory.
# Only do that if necessary to keep visible output short.
.if ${.CURDIR} == ${.OBJDIR}
PYTHON =	python2.7 -u
SCAPYPATH =
.else
PYTHON =	PYTHONPATH=${.OBJDIR} python2.7 -u
SCAPYPATH =	${.CURDIR}/
.endif

.for s in ${SCAPYS}
run-regress-$s: $s addr.py stamp-pfctl
	@echo '\n======== $@ ========'
	time SUDO=${SUDO} PYTHON="${PYTHON}" perl ${PERLINC} ${PERLPATH}scapy.pl splice ${FAKE_NET_ADDR} ${REMOTE_ADDR} ${REMOTE_SSH} ${SCAPYPATH}$s
.endfor

.if ! empty(PF_ANCHOR)
TARGETS +=	cleanup
run-regress-cleanup:
	@echo '\n======== $@ ========'
	${SUDO} pfctl -a regress -Fr
	rm -f stamp-pfctl
.endif

CLEANFILES +=		addr.py *.pyc *.log stamp-*

.PHONY: check-setup check-setup-local check-setup-remote

# Check wether the address, route and remote setup is correct
check-setup: check-setup-local check-setup-remote

check-setup-local:
	@echo '\n======== $@ ========'
	ping -n -c 1 ${LOCAL_ADDR}  # LOCAL_ADDR
	route -n get -inet ${LOCAL_ADDR} | grep -q 'flags: .*LOCAL'  # LOCAL_ADDR
	ping -n -c 1 ${REMOTE_ADDR}  # REMOTE_ADDR
	route -n get -inet ${REMOTE_ADDR} | fgrep -q 'interface: ${LOCAL_IF}'  # REMOTE_ADDR LOCAL_IF
	! ping -n -c 1 -w 1 ${FAKE_NET_ADDR}  # FAKE_NET_ADDR
.for ip in FAKE_NET FAKE_NET_ADDR
	route -n get -inet ${${ip}} | grep -q 'flags: .*BLACKHOLE'  # ${ip}
.endfor

check-setup-remote:
	@echo '\n======== $@ ========'
	ssh ${REMOTE_SSH} ping -n -c 1 ${REMOTE_ADDR}  # REMOTE_ADDR
	ssh ${REMOTE_SSH} route -n get -inet ${REMOTE_ADDR} | grep -q 'flags: .*LOCAL'  # REMOTE_ADDR
	ssh ${REMOTE_SSH} ping -n -c 1 ${LOCAL_ADDR}  # LOCAL_ADDR
.for ip in FAKE_NET FAKE_NET_ADDR
	ssh ${REMOTE_SSH} route -n get -inet ${${ip}} | fgrep -q 'gateway: ${LOCAL_ADDR}'  # ${ip} LOCAL_ADDR
.endfor

.include <bsd.regress.mk>
