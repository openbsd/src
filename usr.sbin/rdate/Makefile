#	$OpenBSD: Makefile,v 1.6 2002/07/27 08:47:19 jakob Exp $

PROG=	rdate
SRCS=	rdate.c rfc868time.c ntp.c ntpleaps.c
CFLAGS+=-Wall -Werror
DPADD+= ${LIBUTIL}
LDADD+= -lutil

MAN=	rdate.8

.include <bsd.prog.mk>
