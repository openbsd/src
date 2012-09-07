#	$OpenBSD: Makefile,v 1.13 2012/09/07 00:33:24 deraadt Exp $

PROG=	ifconfig
SRCS=	ifconfig.c brconfig.c
MAN=	ifconfig.8

LDADD=	-lutil
DPADD=	${LIBUTIL}

CPPFLAGS+=-DINET6

.include <bsd.prog.mk>
