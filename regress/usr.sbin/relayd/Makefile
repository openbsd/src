#	$OpenBSD: Makefile,v 1.10 2014/12/31 01:25:07 bluhm Exp $

# The following ports must be installed for the regression tests:
# p5-IO-Socket-INET6	object interface for AF_INET and AF_INET6 domain sockets
# p5-Socket6		Perl defines relating to AF_INET6 sockets
# p5-IO-Socket-SSL	perl interface to SSL sockets
#
# Check wether all required perl packages are installed.  If some
# are missing print a warning and skip the tests, but do not fail.

PERL_REQUIRE !=	perl -Mstrict -Mwarnings -e ' \
    eval { require IO::Socket::INET6 } or print $@; \
    eval { require Socket6 } or print $@; \
    eval { require IO::Socket::SSL } or print $@; \
'
.if ! empty (PERL_REQUIRE)
regress:
	@echo "${PERL_REQUIRE}"
	@echo install these perl packages for additional tests
.endif

# Fill out these variables if you want to test relayd with
# the relayd process running on a remote machine.  You have to specify
# a local and remote ip address for the tcp connections.  To control
# the remote machine you need a hostname for ssh to log in.  All the
# test files must be in the same directory local and remote.

LOCAL_ADDR ?=
REMOTE_ADDR ?=
REMOTE_SSH ?=

# Automatically generate regress targets from test cases in directory.

ARGS !=			cd ${.CURDIR} && ls args-*.pl
TARGETS ?=		${ARGS}
REGRESS_TARGETS =	${TARGETS:S/^/run-regress-/}
CLEANFILES +=		*.log relayd.conf ktrace.out stamp-*
CLEANFILES +=		*.pem *.req *.crt *.key *.srl

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
	@echo '\n======== $@ ========'
.if empty (REMOTE_SSH)
	time SUDO=${SUDO} KTRACE=${KTRACE} RELAYD=${RELAYD} perl ${PERLINC} ${PERLPATH}relayd.pl copy ${PERLPATH}$a
	time SUDO=${SUDO} KTRACE=${KTRACE} RELAYD=${RELAYD} perl ${PERLINC} ${PERLPATH}relayd.pl splice ${PERLPATH}$a
.else
	ssh -t ${REMOTE_SSH} ${SUDO} true
	time SUDO=${SUDO} KTRACE=${KTRACE} RELAYD=${RELAYD} perl ${PERLINC} ${PERLPATH}remote.pl copy ${LOCAL_ADDR} ${REMOTE_ADDR} ${REMOTE_SSH} ${PERLPATH}$a
	time SUDO=${SUDO} KTRACE=${KTRACE} RELAYD=${RELAYD} perl ${PERLINC} ${PERLPATH}remote.pl splice ${LOCAL_ADDR} ${REMOTE_ADDR} ${REMOTE_SSH} ${PERLPATH}$a
.endif
.endfor

# create certificates for TLS

.for ip in ${REMOTE_ADDR} 127.0.0.1
${ip}.crt:
	openssl req -batch -new -subj /L=OpenBSD/O=relayd-regress/OU=relay/CN=${ip}/ -nodes -newkey rsa -keyout ${ip}.key -x509 -out $@
.if empty (REMOTE_SSH)
	${SUDO} cp 127.0.0.1.crt /etc/ssl/
	${SUDO} cp 127.0.0.1.key /etc/ssl/private/
.else
	scp ${REMOTE_ADDR}.crt root@${REMOTE_SSH}:/etc/ssl/
	scp ${REMOTE_ADDR}.key root@${REMOTE_SSH}:/etc/ssl/private/
.endif
.endfor

ca.crt:
	openssl req -batch -new -subj /L=OpenBSD/O=relayd-regress/OU=ca/CN=root/ -nodes -newkey rsa -keyout ca.key -x509 -out ca.crt

server.req:
	openssl req -batch -new -subj /L=OpenBSD/O=relayd-regress/OU=server/CN=localhost/ -nodes -newkey rsa -keyout server.key -out server.req

server.crt: ca.crt server.req
	openssl x509 -CAcreateserial -CAkey ca.key -CA ca.crt -req -in server.req -out server.crt

${REGRESS_TARGETS:M*ssl*} ${REGRESS_TARGETS:M*https*}: server.crt
.if empty (REMOTE_SSH)
${REGRESS_TARGETS:M*ssl*} ${REGRESS_TARGETS:M*https*}: 127.0.0.1.crt
.else
${REGRESS_TARGETS:M*ssl*} ${REGRESS_TARGETS:M*https*}: ${REMOTE_ADDR}.crt
.endif

# make perl syntax check for all args files

.PHONY: syntax

syntax: stamp-syntax

stamp-syntax: ${ARGS}
.for a in ${ARGS}
	@perl -c ${PERLPATH}$a
.endfor
	@date >$@

.include <bsd.regress.mk>
