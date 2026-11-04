#	$OpenBSD: Makefile,v 1.1 2026/11/04 15:42:44 op Exp $

# The following ports must be installed for the regression tests:
# p5-Socket6		Perl defines relating to AF_INET6 sockets
# p5-IO-Socket-SSL	Perl interface to SSL sockets
#
# Check wether all required perl packages are installed.  If some
# are missing print a warning and skip the tests, but do not fail.

PERL_REQUIRE !=	perl -Mstrict -Mwarnings -e ' \
    eval { require Socket6 } or print $@; \
    eval { require IO::Socket::SSL } or print $@; \
'
.if ! empty (PERL_REQUIRE)
regress:
	@echo "${PERL_REQUIRE}"
	@echo 'run "pkg_add p5-Socket6 p5-IO-Socket-SSL"'
	@echo SKIPPED
.endif

REGRESS_SETUP_ONCE +=	setup
setup:
	${SUDO} true
	${SUDO} /etc/rc.d/smtpd stop

REGRESS_CLEANUP +=	cleanup
cleanup:
	-${SUDO} /etc/rc.d/smtpd restart

# Automatically generate regress targets from test cases in directory.

ARGS !=			cd ${.CURDIR} && ls args-*.pl
CLEANFILES +=		*.log smtpd.conf aliases-test.txt vusers-db vusers-file.txt
CLEANFILES +=		ktrace.out stamp-*

# Set variables so that make runs with and without obj directory.
# Only do that if necessary to keep visible output short.

.if ${.CURDIR} == ${.OBJDIR}
PERLINC =	-I.
PERLPATH =
.else
PERLINC =	-I${.CURDIR}
PERLPATH =	${.CURDIR}/
.endif

# The arg tests take a perl hash with arguments controlling the
# test parameters.  Generally they consist of client, relayd, server.

.for test in ${ARGS}
REGRESS_TARGETS +=	run-${test}
run-${test}: ${test}
	time SUDO="${SUDO}" KTRACE=${KTRACE} SMTPD=${SMTPD} CURDIR=${.CURDIR} \
		perl ${PERLINC} ${PERLPATH}smtpd.pl ${PERLPATH}${test}
.endfor

# make perl syntax check for all args files

.PHONY: syntax

syntax: stamp-syntax

stamp-syntax: ${ARGS}
.for test in ${ARGS}
	@perl -c ${PERLPATH}${test}
.endfor
	@date >$@

.include <bsd.regress.mk>
