#	$OpenBSD: Makefile,v 1.9 2016/08/02 17:00:09 jca Exp $

PROG=	rtadvd
SRCS=	rtadvd.c advcap.c if.c config.c dump.c log.c

CFLAGS+=-Wall
MAN=	rtadvd.8 rtadvd.conf.5

LDADD=	-levent
DPADD+=	${LIBEVENT}

.include <bsd.prog.mk>
