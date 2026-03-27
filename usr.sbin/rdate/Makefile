#	$OpenBSD: Makefile,v 1.8 2026/03/27 14:33:58 deraadt Exp $

PROG=	rdate
SRCS=	rdate.c ntp.c ntpleaps.c
CFLAGS+=-Wall
DPADD+= ${LIBUTIL}
LDADD+= -lutil

MAN=	rdate.8

.include <bsd.prog.mk>
