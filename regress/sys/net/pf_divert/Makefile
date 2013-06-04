#	$OpenBSD: Makefile,v 1.2 2013/06/04 04:17:42 bluhm Exp $

# The following ports must be installed for the regression tests:
# p5-IO-Socket-INET6	object interface for AF_INET and AF_INET6 domain sockets
# p5-Socket6		Perl defines relating to AF_INET6 sockets


TARGETS ?=		${ARGS}
.if empty (TARGETS)
ARGS !=			cd ${.CURDIR} && ls args-*.pl
.endif
REGRESS_TARGETS =	${TARGETS:S/^/run-regress-/}
CLEANFILES +=		*.log ktrace.out stamp-*

# Fill out these variables as you have to test divert with the pf
# kernel running on a remote machine.  You have to specify a local
# and remote ip address for the test connections.  The fake ip address
# will be routed via the remote address to test divert with non-existing
# addresses.  To control the remote machine you need a hostname for
# ssh to log in.  All the test files must be in the same directory
# local and remote.
# You must have an anchor "regress" for the divert rules in the pf.conf
# of the remote machine.  The kernel of the remote machine gets testet.

LOCAL_ADDR ?= 10.188.50.10
REMOTE_ADDR ?= 10.188.50.50
FAKE_ADDR ?= 10.188.50.51
LOCAL_ADDR6 ?= fdd7:e83e:66bc:211:fce1:baff:fed1:561f
REMOTE_ADDR6 ?= fdd7:e83e:66bc:211:5054:ff:fe12:3450
FAKE_ADDR6 ?= fdd7:e83e:66bc:211:5054:ff:fe12:3451
REMOTE_SSH ?= q0

.MAIN: all

.if make(regress) || make(all)
.BEGIN:
.if empty (REMOTE_SSH)
	@echo "remote test target host not set"; false
.endif
	@echo
	${SUDO} true
	ssh -t q0 ${SUDO} true
	-${SUDO} route -n delete -inet -host ${FAKE_ADDR} 2>/dev/null
	${SUDO} route -n add -inet -host ${FAKE_ADDR} ${REMOTE_ADDR}
	-${SUDO} route -n delete -inet6 -host ${FAKE_ADDR6} 2>/dev/null
	${SUDO} route -n add -inet6 -host ${FAKE_ADDR6} ${REMOTE_ADDR6}
.endif

# Set variables so that make runs with and without obj directory.
# Only do that if necessary to keep visible output short.

.if ${.CURDIR} == ${.OBJDIR}
PERLINC =	-I.
PERLPATH =
.else
PERLINC =	-I${.CURDIR}
PERLPATH =	${.CURDIR}/
.endif

# The arg tests take a perl hash with arguments controlling the test
# parameters.  The remote.pl test has local client or server and the
# diverted process is running on the remote machine reachable with
# ssh.

.for a in ${ARGS}
run-regress-$a: $a
.if !empty (LOCAL_ADDR)
	@echo
	time ${SUDO} SUDO=${SUDO} perl ${PERLINC} ${PERLPATH}remote.pl inet ${LOCAL_ADDR} ${FAKE_ADDR} ${REMOTE_SSH} ${PERLPATH}$a
.endif
.if !empty (LOCAL_ADDR6)
	@echo
	time ${SUDO} SUDO=${SUDO} perl ${PERLINC} ${PERLPATH}remote.pl inet6 ${LOCAL_ADDR6} ${FAKE_ADDR6} ${REMOTE_SSH} ${PERLPATH}$a
.endif
.endfor

# make perl syntax check for all args files

.PHONY: syntax

syntax: stamp-syntax

stamp-syntax: ${ARGS}
.for a in ${ARGS}
	@perl -c ${PERLPATH}$a
.endfor
	@date >$@

.include <bsd.regress.mk>
