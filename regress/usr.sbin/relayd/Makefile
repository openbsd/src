#	$OpenBSD: Makefile,v 1.27 2026/09/21 01:13:12 rsadowski Exp $

# The following ports must be installed for the regression tests:
# p5-Socket6		Perl defines relating to AF_INET6 sockets
# p5-IO-Socket-SSL	perl interface to SSL sockets
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
.if empty (REMOTE_SSH)
	${SUDO} true
.else
	ssh -t ${REMOTE_SSH} ${SUDO} true
.endif

# Fill out these variables if you want to test relayd with
# the relayd process running on a remote machine.  You have to specify
# a local and remote ip address for the tcp connections.  To control
# the remote machine you need a hostname for ssh to log in.  All the
# test files must be in the same directory local and remote.

LOCAL_ADDR ?=
REMOTE_ADDR ?=
REMOTE_SSH ?=

ARGS_CONFIGS !=		cd ${.CURDIR}/configs && ls *.conf

# Automatically generate regress targets from test cases in directory.
# TLS tests are run once per cert type (RSA and EC) via CERTBASE.

ARGS_TLS !=		cd ${.CURDIR} && ls args-*.pl | grep -E 'ssl|https'
ARGS !=			cd ${.CURDIR} && ls args-*.pl | grep -vE 'ssl|https'
CERTBASES =		127.0.0.1 127.0.0.1-ec
CLEANFILES +=		*.log relayd.conf ktrace.out stamp-*
CLEANFILES +=		*.pem *.req *.crt *.key *.srl

# Set variables so that make runs with and without obj directory.
# Only do that if necessary to keep visible output short.

.if ${.CURDIR} == ${.OBJDIR}
PERLINC =	-I.
PERLPATH =
.else
PERLINC =	-I${.CURDIR}
PERLPATH =	${.CURDIR}/
.endif

# syntax checks only
.for config in ${ARGS_CONFIGS}
REGRESS_TARGETS +=	run-config-check-${config}
run-config-check-${config}: ${.CURDIR}/configs/${config} 127.0.0.1.crt 127.0.0.1.key
.if empty (RELAYD)
	${SUDO} /usr/sbin/relayd -n -f ${.CURDIR}/configs/${config}
.else
	${SUDO} ${RELAYD} -n -f ${.CURDIR}/configs/${config}
.endif
.endfor

# The arg tests take a perl hash with arguments controlling the
# test parameters.  Generally they consist of client, relayd, server.

.for test in ${ARGS}
REGRESS_TARGETS +=	run-${test}
run-${test}: ${test}
.if empty (REMOTE_SSH)
	time SUDO="${SUDO}" KTRACE=${KTRACE} RELAYD=${RELAYD} \
		perl ${PERLINC} ${PERLPATH}relayd.pl copy ${PERLPATH}${test}
	time SUDO="${SUDO}" KTRACE=${KTRACE} RELAYD=${RELAYD} \
		perl ${PERLINC} ${PERLPATH}relayd.pl splice ${PERLPATH}${test}
.else
	time SUDO="${SUDO}" KTRACE=${KTRACE} RELAYD=${RELAYD} \
		perl ${PERLINC} ${PERLPATH}remote.pl copy ${LOCAL_ADDR} \
		${REMOTE_ADDR} ${REMOTE_SSH} ${PERLPATH}${test}
	time SUDO="${SUDO}" KTRACE=${KTRACE} RELAYD=${RELAYD} \
		perl ${PERLINC} ${PERLPATH}remote.pl splice ${LOCAL_ADDR} \
		${REMOTE_ADDR} ${REMOTE_SSH} ${PERLPATH}${test}
.endif
.endfor

# TLS tests run once per cert type.  CERTBASE selects the keypair files.
.for test in ${ARGS_TLS}
.for cert in ${CERTBASES}
REGRESS_TARGETS +=	run-${cert}-${test}
run-${cert}-${test}: ${test}
.if empty (REMOTE_SSH)
	time SUDO="${SUDO}" KTRACE=${KTRACE} RELAYD=${RELAYD} CERTBASE=${cert} \
		perl ${PERLINC} ${PERLPATH}relayd.pl copy ${PERLPATH}${test}
	time SUDO="${SUDO}" KTRACE=${KTRACE} RELAYD=${RELAYD} CERTBASE=${cert} \
		perl ${PERLINC} ${PERLPATH}relayd.pl splice ${PERLPATH}${test}
.else
	scp ${cert}.crt ${REMOTE_SSH}:${cert}.crt
	scp ${cert}.key ${REMOTE_SSH}:${cert}.key
	time SUDO="${SUDO}" KTRACE=${KTRACE} RELAYD=${RELAYD} CERTBASE=${cert} \
		perl ${PERLINC} ${PERLPATH}remote.pl copy ${LOCAL_ADDR} \
		${REMOTE_ADDR} ${REMOTE_SSH} ${PERLPATH}${test}

	time SUDO="${SUDO}" KTRACE=${KTRACE} RELAYD=${RELAYD} CERTBASE=${cert} \
		perl ${PERLINC} ${PERLPATH}remote.pl splice ${LOCAL_ADDR} \
		${REMOTE_ADDR} ${REMOTE_SSH} ${PERLPATH}${test}
.endif
.endfor
.endfor

# create certificates for TLS

.for ip in ${REMOTE_ADDR} 127.0.0.1
${ip}.crt: ca.crt client-ca.crt
	openssl req -batch -new \
	    -subj /L=OpenBSD/O=relayd-regress/OU=relayd/CN=${ip}/ \
	    -nodes -newkey rsa -keyout ${ip}.key -x509 \
	    -out $@
.if !empty (REMOTE_SSH)
	scp ${REMOTE_ADDR}.crt ${REMOTE_SSH}:
	scp ${REMOTE_ADDR}.key ${REMOTE_SSH}:
	scp ca.crt ca.key ${REMOTE_SSH}:
	scp client-ca.crt client-ca.key ${REMOTE_SSH}:
.endif

${ip}-ec.crt:
	openssl ecparam -name secp384r1 -genkey -noout \
	    -out ${ip}-ec.key
	openssl req -batch -new -x509 \
	    -subj /L=OpenBSD/O=relayd-regress/OU=relayd/CN=${ip}/ \
	    -key ${ip}-ec.key \
	    -out $@
.if !empty (REMOTE_SSH)
	scp ${REMOTE_ADDR}-ec.crt ${REMOTE_SSH}:
	scp ${REMOTE_ADDR}-ec.key ${REMOTE_SSH}:
.endif
.endfor

ca.crt client-ca.crt:
	openssl req -batch -new \
	    -subj /L=OpenBSD/O=relayd-regress/OU=${@:R}/CN=root/ \
	    -nodes -newkey rsa -keyout ${@:R}.key -x509 \
	    -out $@

server.req client.req:
	openssl req -batch -new \
	    -subj /L=OpenBSD/O=relayd-regress/OU=${@:R}/CN=localhost/ \
	    -nodes -newkey rsa -keyout ${@:R}.key \
	    -out $@

server.crt: ca.crt server.req
	openssl x509 -CAcreateserial -CAkey ca.key -CA ca.crt \
	    -req -in server.req -out server.crt

client.crt: client-ca.crt client.req
	openssl x509 -CAcreateserial -CAkey client-ca.key -CA client-ca.crt \
	    -req -in client.req -out client.crt

${REGRESS_TARGETS:M*ssl*} ${REGRESS_TARGETS:M*https*}: server.crt client.crt
.if empty (REMOTE_SSH)
${REGRESS_TARGETS:M*127.0.0.1-args-*}: 127.0.0.1.crt
${REGRESS_TARGETS:M*127.0.0.1-ec-args-*}: 127.0.0.1-ec.crt
.else
${REGRESS_TARGETS:M*127.0.0.1-args-*}: ${REMOTE_ADDR}.crt
${REGRESS_TARGETS:M*127.0.0.1-ec-args-*}: ${REMOTE_ADDR}-ec.crt
.endif

# make perl syntax check for all args files

.PHONY: syntax

syntax: stamp-syntax

stamp-syntax: ${ARGS} ${ARGS_TLS}
.for test in ${ARGS} ${ARGS_TLS}
	@perl -c ${PERLPATH}${test}
.endfor
	@date >$@

.include <bsd.regress.mk>
