#	$OpenBSD: Makefile,v 1.3 1997/04/04 08:59:39 deraadt Exp $
#	$NetBSD: Makefile,v 1.3 1996/02/22 06:59:15 thorpej Exp $

PROG=	rdate
SRCS=	rdate.c
DPADD+= ${LIBUTIL}
LDADD+= -lutil

MAN=	rdate.8

.include <bsd.prog.mk>
