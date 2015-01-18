#	$OpenBSD: Makefile,v 1.7 2015/01/18 19:37:59 bluhm Exp $

PROG=	syslogd
SRCS=	syslogd.c ttymsg.c privsep.c privsep_fdpass.c ringbuf.c evbuffer_tls.c
MAN=	syslogd.8 syslog.conf.5
LDADD=	-levent -ltls -lssl -lcrypto
DPADD=	${LIBEVENT} ${LIBTLS} ${LIBSSL} ${LIBCRYPTO}

.include <bsd.prog.mk>
