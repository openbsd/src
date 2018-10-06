#	$OpenBSD: Makefile,v 1.24 2018/10/06 10:52:24 bluhm Exp $

# The following ports must be installed for the regression tests:
# p5-IO-Socket-INET6	object interface for AF_INET and AF_INET6 domain sockets
# p5-Socket6		Perl defines relating to AF_INET6 sockets
# p5-IO-Socket-SSL	perl interface to SSL sockets
# p5-BSD-Resource	BSD process resource limit and priority functions
#
# This package enables additional interoperability tests
# rsyslog		syslog daemon supporting databases, TCP, SSL, RELP
#
# Check wether all required perl packages are installed.  If some
# are missing print a warning and skip the tests, but do not fail.

PERL_REQUIRE !=	perl -Mstrict -Mwarnings -e ' \
    eval { require IO::Socket::INET6 } or print $@; \
    eval { require Socket6 } or print $@; \
    eval { require IO::Socket::SSL } or print $@; \
    eval { require BSD::Resource } or print $@; \
'
.if ! empty (PERL_REQUIRE)
regress:
	@echo "${PERL_REQUIRE}"
	@echo install these perl packages for additional tests
	@echo SKIPPED
.endif

# Automatically generate regress targets from test cases in directory.

PROGS =			ttylog
PERLS =			Client.pm Proc.pm RSyslogd.pm Server.pm \
			Syslogc.pm   Syslogd.pm funcs.pl syslogd.pl
ARGS !=			cd ${.CURDIR} && ls args-*.pl
.if exists (/usr/local/sbin/rsyslogd)
TARGETS ?=		${ARGS}
.else
TARGETS ?=		${ARGS:Nargs-rsyslog*}
.endif
REGRESS_TARGETS =	${TARGETS:S/^/run-/}
LDFLAGS +=		-lutil
CLEANFILES +=		*.log *.log.? *.conf ktrace.out stamp-*
CLEANFILES +=		*.out *.sock *.ktrace *.fstat ttylog *.ph */*.ph
CLEANFILES +=		*.pem *.req *.key *.crt *.srl empty toobig diskimage

.MAIN: all

REGRESS_SETUP_ONCE +=	setup
setup:
	@echo '\n======== $@ ========'
	[ -z "${SUDO}" ] || ${SUDO} true
	${SUDO} /etc/rc.d/syslogd stop

REGRESS_CLEANUP +=	cleanup
cleanup:
	@echo '\n======== $@ ========'
	-${SUDO} /etc/rc.d/syslogd restart

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
# test parameters.  Generally they consist of client, syslogd, server.

.for a in ${ARGS}
run-$a: $a
	@echo '\n======== $@ ========'
	time SUDO=${SUDO} KTRACE=${KTRACE} SYSLOGD=${SYSLOGD} perl ${PERLINC} ${PERLPATH}syslogd.pl ${PERLPATH}$a
.endfor

# create certificates for TLS

127.0.0.1.crt:
	openssl req -batch -new -subj /L=OpenBSD/O=syslogd-regress/OU=syslogd/CN=127.0.0.1/ -nodes -newkey rsa -keyout 127.0.0.1.key -x509 -out $@
	${SUDO} rm -f /etc/ssl/private/127.0.0.1:6514.key
	${SUDO} rm -f /etc/ssl/127.0.0.1:6514.crt
	${SUDO} rm -f /etc/ssl/private/127.0.0.1.key
	${SUDO} rm -f /etc/ssl/127.0.0.1.crt
	${SUDO} cp 127.0.0.1.key /etc/ssl/private/127.0.0.1.key
	${SUDO} cp 127.0.0.1.crt /etc/ssl/127.0.0.1.crt
	${SUDO} cp 127.0.0.1.key /etc/ssl/private/::1.key
	${SUDO} cp 127.0.0.1.crt /etc/ssl/::1.crt
	${SUDO} cp 127.0.0.1.key /etc/ssl/private/localhost.key
	${SUDO} cp 127.0.0.1.crt /etc/ssl/localhost.crt

ca.crt fake-ca.crt:
	openssl req -batch -new -subj /L=OpenBSD/O=syslogd-regress/OU=ca/CN=root/ -nodes -newkey rsa -keyout ${@:R}.key -x509 -out $@

client.req server.req:
	openssl req -batch -new -subj /L=OpenBSD/O=syslogd-regress/OU=${@:R}/CN=localhost/ -nodes -newkey rsa -keyout ${@:R}.key -out $@

client.crt server.crt: ca.crt ${@:R}.req
	openssl x509 -CAcreateserial -CAkey ca.key -CA ca.crt -req -in ${@:R}.req -out $@

empty:
	true >$@

toobig:
	dd if=/dev/zero of=$@ bs=1 count=1 seek=50M

sys/syscall.ph: /usr/include/sys/syscall.h
	cd /usr/include && h2ph -h -d ${.OBJDIR} ${@:R:S/$/.h/}

# Create a full file system on vnd to trigger ENOSPC error.

.PHONY: disk mount unconfig

disk: unconfig
	dd if=/dev/zero of=diskimage bs=512 count=4k
	vnconfig vnd0 diskimage
	newfs -b 4096 -f 2048 -m 0 vnd0c

mount: disk
	mkdir -p /mnt/regress-syslogd
	mount /dev/vnd0c /mnt/regress-syslogd

unconfig:
	-umount -f /dev/vnd0c 2>/dev/null || true
	-rmdir /mnt/regress-syslogd 2>/dev/null || true
	-vnconfig -u vnd0 2>/dev/null || true

stamp-filesystem:
	@echo '\n======== $@ ========'
	${SUDO} ${.MAKE} -C ${.CURDIR} mount
	${SUDO} chmod 1777 /mnt/regress-syslogd
	date >$@

REGRESS_CLEANUP +=	cleanup-filesystem
cleanup-filesystem:
	@echo '\n======== $@ ========'
	rm -f stamp-filesystem
	-${SUDO} umount /mnt/regress-syslogd
	-${SUDO} ${.MAKE} -C ${.CURDIR} unconfig

${REGRESS_TARGETS:M*filesystem*}: stamp-filesystem
${REGRESS_TARGETS:M*tls*}: client.crt server.crt 127.0.0.1.crt
${REGRESS_TARGETS:M*multilisten*}: 127.0.0.1.crt
${REGRESS_TARGETS:M*empty*}: empty
${REGRESS_TARGETS:M*toobig*}: toobig
${REGRESS_TARGETS:M*fake*}: fake-ca.crt
${REGRESS_TARGETS:M*sendsyslog*}: sys/syscall.ph
${REGRESS_TARGETS}: ttylog

# make perl syntax check for all args files

.PHONY: syntax libevent

syntax: stamp-syntax

stamp-syntax: ${PERLS} ${ARGS}
.for p in ${PERLS}
	@perl -c ${PERLINC} ${PERLPATH}$p
.endfor
.for a in ${ARGS}
	@perl -c ${PERLPATH}$a
.endfor
	@date >$@

# run the tests with all variants of libevent backend
libevent:
	cd ${.CURDIR} && EVENT_NOKQUEUE=1 EVENT_NOPOLL=1 ${MAKE} regress
	cd ${.CURDIR} && EVENT_NOKQUEUE=1 EVENT_NOSELECT=1 ${MAKE} regress
	cd ${.CURDIR} && EVENT_NOPOLL=1 EVENT_NOSELECT=1 ${MAKE} regress

.include <bsd.regress.mk>
