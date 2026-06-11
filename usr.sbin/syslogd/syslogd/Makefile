#	$OpenBSD: Makefile,v 1.2 2026/06/11 18:28:46 bluhm Exp $

.PATH:	${.CURDIR}/..

PROG=	syslogd
BINDIR=	/usr/sbin
SRCS=	evbuffer_tls.c log.c parsemsg.c privsep.c \
	privsep_fdpass.c ringbuf.c syslogd.c ttymsg.c
LDADD=	-levent -ltls -lssl -lcrypto
DPADD=	${LIBEVENT} ${LIBTLS} ${LIBSSL} ${LIBCRYPTO}
MAN=	syslogd.8 syslog.conf.5

.include <bsd.prog.mk>
