#	$OpenBSD: Makefile,v 1.5 2002/05/16 10:46:34 jakob Exp $

PROG=	rdate
SRCS=	rdate.c rfc868time.c ntp.c
DPADD+= ${LIBUTIL}
LDADD+= -lutil

MAN=	rdate.8

.include <bsd.prog.mk>
