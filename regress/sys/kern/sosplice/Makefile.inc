#	$OpenBSD: Makefile.inc,v 1.4 2019/07/12 19:31:29 bluhm Exp $

# The following ports must be installed for the regression tests:
# p5-BSD-Socket-Splice	perl interface to OpenBSD socket splicing
# p5-IO-Socket-INET6	object interface for AF_INET and AF_INET6 domain sockets
# p5-Socket6		Perl defines relating to AF_INET6 sockets
#
# Check wether all required perl packages are installed.  If some
# are missing print a warning and skip the tests, but do not fail.

PERL_REQUIRE !=	perl -Mstrict -Mwarnings -e ' \
    eval { require BSD::Socket::Splice } or print $@; \
    eval { require IO::Socket::INET6 } or print $@; \
    eval { require Socket6 } or print $@; \
'
.if ! empty (PERL_REQUIRE) && ! defined (TARGETS)
regress:
	@echo "${PERL_REQUIRE}"
	@echo install these perl packages for additional tests
.endif

# Fill out these variables if you want to test socket splicing with
# the relay process running on a remote machine.  You have to specify
# a local and remote ip address for the tcp connections.  To control
# the remote machine you need a hostname for ssh to log in.  All the
# test files must be in the same directory local and remote.

LOCAL_ADDR ?=
REMOTE_ADDR ?=
REMOTE_SSH ?=

# Automatically generate regress targets from test cases in directory.

.if ! defined (TARGETS)
ARGS !=			cd ${.CURDIR} && ls args-*.pl
TARGETS ?=		${ARGS}
.endif
REGRESS_TARGETS =	${TARGETS:S/^/run-/}
CLEANFILES +=		*.log ktrace.out stamp-*

# Set variables so that make runs with and without obj directory.
# Only do that if necessary to keep visible output short.

.if ${.CURDIR} == ${.OBJDIR}
PERLINC =	-I.. -I.
PERLPATH =
.else
PERLINC =	-I${.CURDIR}/..
PERLPATH =	${.CURDIR}/
.endif

# make perl syntax check for all args files

.PHONY: syntax

syntax: stamp-syntax

stamp-syntax: ${ARGS}
.for a in ${ARGS}
	@perl -c ${PERLPATH}$a
.endfor
	@date >$@
