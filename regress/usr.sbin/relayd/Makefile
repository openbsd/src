#	$OpenBSD: Makefile,v 1.2 2011/09/02 17:02:10 bluhm Exp $

# The following ports must be installed for the regression tests:
# p5-IO-Socket-INET6	object interface for AF_INET and AF_INET6 domain sockets
# p5-Socket6		Perl defines relating to AF_INET6 sockets
# p5-IO-Socket-SSL	perl interface to SSL sockets

ARGS !=			cd ${.CURDIR} && ls args-*.pl
TARGETS ?=		${ARGS}
REGRESS_TARGETS =	${TARGETS:S/^/run-regress-/}
CLEANFILES =		*.log *.pem relayd.conf ktrace.out

# Set variables so that make runs with and without obj directory.
# Only do that if necessary to keep visible output short.

.if ${.CURDIR} == ${.OBJDIR}
PERLINC =
PERLPATH =
.else
PERLINC =	-I${.CURDIR}
PERLPATH =	${.CURDIR}/
.endif

# The arg tests take a perl hash with arguments controlling the
# test parameters.  Generally they consist of client, relayd, server.

.for a in ${ARGS}
run-regress-$a: $a
	time SUDO=${SUDO} perl ${PERLINC} ${PERLPATH}relayd.pl copy ${PERLPATH}$a
	time SUDO=${SUDO} perl ${PERLINC} ${PERLPATH}relayd.pl splice ${PERLPATH}$a
.endfor

/etc/ssl/127.0.0.1.crt:
	${SUDO} openssl req -batch -new -nodes -newkey rsa -keyout /etc/ssl/private/127.0.0.1.key -subj /CN=127.0.0.1/ -x509 -out $@

server-cert.pem:
	openssl req -batch -new -nodes -newkey rsa -keyout server-key.pem -subj /CN=localhost/ -x509 -out $@

${REGRESS_TARGETS:M*ssl*} ${REGRESS_TARGETS:M*https*}: \
    /etc/ssl/127.0.0.1.crt server-cert.pem

.include <bsd.regress.mk>
