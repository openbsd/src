#	$OpenBSD: Makefile,v 1.1 2009/10/28 05:17:34 deraadt Exp $

COPTS+=-Os
PROG=	mt
SRCS=	mt.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
LDSTATIC=-static
MAN=

.PATH:	${.CURDIR}/../../../bin/mt

.include <bsd.prog.mk>
