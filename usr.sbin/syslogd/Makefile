#	$OpenBSD: Makefile,v 1.8 2017/03/16 23:55:19 bluhm Exp $

PROG=	syslogd
SRCS=	evbuffer_tls.c log.c privsep.c privsep_fdpass.c	ringbuf.c syslogd.c \
	ttymsg.c
MAN=	syslogd.8 syslog.conf.5
LDADD=	-levent -ltls -lssl -lcrypto
DPADD=	${LIBEVENT} ${LIBTLS} ${LIBSSL} ${LIBCRYPTO}

.include <bsd.prog.mk>
