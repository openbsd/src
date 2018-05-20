#	$OpenBSD: Makefile,v 1.4 2017/10/11 17:21:44 florian Exp $

# The following ports must be installed:
#
# python-2.7          interpreted object-oriented programming language
# scapy               powerful interactive packet manipulation in python

RTABLE		?=	1
PAIR1		?=	pair1
PAIR2		?=	pair2
CTR_SOCK	?=	${.OBJDIR}/slaacd.sock

.if ! (make(clean) || make(cleandir) || make(obj))
# Check whether all required python packages are installed.  If some
# are missing print a warning and skip the tests, but do not fail.
PYTHON_IMPORT != python2.7 -c 'from scapy.all import *' 2>&1 || true
AUTOCONF6 != sh -c "ifconfig | fgrep AUTOCONF6 2>&1" || true
INTERFACES != sh -c "ifconfig ${PAIR1} 2>/dev/null; \
    ifconfig ${PAIR2} 2>/dev/null" || true
.endif

.if ! empty(PYTHON_IMPORT)
regress:
	@echo '${PYTHON_IMPORT}'
	@echo install python and the scapy module for additional tests
	@echo SKIPPED
.endif

.if ! empty(AUTOCONF6)
regress:
	@echo 'AUTOCONF6 interface(s) already present'
	@echo SKIPPED
.endif
.if ! empty(INTERFACES)
regress:
	@echo 'pair(4) interfaces already present'
	@echo SKIPPED
.endif

# Set variables so that make runs with and without obj directory.
# Only do that if necessary to keep visible output short.
.if ${.CURDIR} == ${.OBJDIR}
PYTHON =	python2.7 -B ./
.else
PYTHON =	python2.7 -B ${.CURDIR}/
.endif

setup:
	@echo '\n======== $@ ========'
	ifconfig ${PAIR1} rdomain ${RTABLE} 10.11.12.1/24 up
	ifconfig ${PAIR2} rdomain ${RTABLE} 10.11.12.2/24 up
	ifconfig ${PAIR1} rdomain ${RTABLE} patch ${PAIR2}
	ifconfig ${PAIR1} inet6 rdomain ${RTABLE} eui64
	ifconfig ${PAIR2} inet6 rdomain ${RTABLE} eui64
	ifconfig ${PAIR2} inet6 rdomain ${RTABLE} autoconf
	route -nq -T ${RTABLE} add -host default 10.11.12.1 -reject
	route -T ${RTABLE} exec slaacd -s ${CTR_SOCK}

cleanup:
	@echo '\n======== $@ ========'
	pkill -T ${RTABLE} -xf "slaacd -s ${CTR_SOCK}" || true
.for iface in ${PAIR1} ${PAIR2}
	ifconfig ${iface} destroy 2>/dev/null || true
.endfor

TARGETS	+=	send-solicitation
run-regress-send-solicitation: cleanup setup
	@echo '\n======== $@ ========'
	route -T${RTABLE} exec ${PYTHON}sniff_sol.py ${CTR_SOCK}


TARGETS	+=	parse-ra
run-regress-parse-ra: cleanup setup
	@echo '\n======== $@ ========'
	route -T${RTABLE} exec ${PYTHON}process_ra.py ${PAIR1} ${PAIR2} \
	    ${CTR_SOCK}

TARGETS	+=	cleanup
run-regress-cleanup: cleanup


REGRESS_TARGETS =	${TARGETS:S/^/run-regress-/}

.include <bsd.regress.mk>
