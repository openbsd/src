#	$OpenBSD: Makefile,v 1.6 2008/04/21 20:40:55 rainer Exp $

PROG=	rtadvd
SRCS=	rtadvd.c rrenum.c advcap.c if.c config.c timer.c dump.c log.c

CFLAGS+=-Wall
LDADD+=	-lutil
DPADD+=	${LIBUTIL}
MAN=	rtadvd.8 rtadvd.conf.5

.include <bsd.prog.mk>
