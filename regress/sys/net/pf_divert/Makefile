#	$OpenBSD: Makefile,v 1.5 2013/11/01 18:07:45 bluhm Exp $

# The following ports must be installed for the regression tests:
# p5-IO-Socket-INET6	object interface for AF_INET and AF_INET6 domain sockets
# p5-Socket6		Perl defines relating to AF_INET6 sockets
#
# Check wether all required perl packages are installed.  If some
# are missing print a warning and skip the tests, but do not fail.

PERL_REQUIRE !=	perl -Mstrict -Mwarnings -e ' \
    eval { require IO::Socket::INET6 } or print $@; \
    eval { require Socket6 } or print $@; \
'
.if ! empty(PERL_REQUIRE)
regress:
	@echo "${PERL_REQUIRE}"
	@echo install these perl packages for additional tests
.endif

# Fill out these variables as you have to test divert with the pf
# kernel running on a remote machine.  You have to specify a local
# and remote ip address for the test connections.  The fake ip address
# will be routed via the remote address to test divert with non-existing
# addresses.  To control the remote machine you need a hostname for
# ssh to log in.  All the test files must be in the same directory
# local and remote.
# You must have an anchor "regress" for the divert rules in the pf.conf
# of the remote machine.  The kernel of the remote machine gets testet.

LOCAL_ADDR ?=
REMOTE_ADDR ?=
FAKE_ADDR ?=
LOCAL_ADDR6 ?=
REMOTE_ADDR6 ?=
FAKE_ADDR6 ?=
REMOTE_SSH ?=

.if empty (LOCAL_ADDR) || empty (REMOTE_ADDR) || empty (FAKE_ADDR) || \
    empty (LOCAL_ADDR6) || empty (REMOTE_ADDR6) || empty (FAKE_ADDR6) || \
    empty (REMOTE_SSH)
regress:
	@echo this tests needs a remote machine to operate on
	@echo LOCAL_ADDR REMOTE_ADDR FAKE_ADDR LOCAL_ADDR6
	@echo REMOTE_ADDR6 FAKE_ADDR6 REMOTE_SSH are empty
	@echo fill out these variables for additional tests
.endif

# Automatically generate regress targets from test cases in directory.

ARGS !=			cd ${.CURDIR} && ls args-*.pl
TARGETS ?=		${ARGS}
REGRESS_TARGETS =	${TARGETS:S/^/run-regress-/}
CLEANFILES +=		*.log ktrace.out stamp-*

.MAIN: all

.if ! empty (REMOTE_SSH)
.if make (regress) || make (all)
.BEGIN:
	@echo
	${SUDO} true
	ssh -t ${REMOTE_SSH} ${SUDO} true
.if ! empty (FAKE_ADDR) && ! empty (REMOTE_ADDR)
	-${SUDO} route -n delete -inet -host ${FAKE_ADDR} 2>/dev/null
	${SUDO} route -n add -inet -host ${FAKE_ADDR} ${REMOTE_ADDR}
.endif
.if ! empty (FAKE_ADDR6) && ! empty (REMOTE_ADDR6)
	-${SUDO} route -n delete -inet6 -host ${FAKE_ADDR6} 2>/dev/null
	${SUDO} route -n add -inet6 -host ${FAKE_ADDR6} ${REMOTE_ADDR6}
.endif
.endif
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

.PHONY: syntax check-setup

syntax: stamp-syntax

stamp-syntax: ${ARGS}
.for a in ${ARGS}
	@perl -c ${PERLPATH}$a
.endfor
	@date >$@

# Check wether the address, route and remote setup is correct
check-setup:
	ping -n -c 1 ${LOCAL_ADDR}
	ping -n -c 1 ${REMOTE_ADDR}
	ping6 -n -c 1 ${LOCAL_ADDR6}
	ping6 -n -c 1 ${REMOTE_ADDR6}
	route get -inet ${FAKE_ADDR} | grep 'if address: ${LOCAL_ADDR}$$'
	route get -inet ${FAKE_ADDR} | grep 'gateway: ${REMOTE_ADDR}$$'
	route get -inet6 ${FAKE_ADDR6} | grep 'if address: ${LOCAL_ADDR6}$$'
	route get -inet6 ${FAKE_ADDR6} | grep 'gateway: ${REMOTE_ADDR6}$$'
	ssh ${REMOTE_SSH} ${SUDO} pfctl -sr | grep '^anchor "regress" all$$'
	ssh ${REMOTE_SSH} ${SUDO} pfctl -si | grep '^Status: Enabled '
	ssh ${REMOTE_SSH} perl -MIO::Socket::INET6 -MIO::Socket::INET6 -e 1

.include <bsd.regress.mk>
