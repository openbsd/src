#	$OpenBSD: Makefile,v 1.12 2012/09/06 19:41:59 tedu Exp $

PROG=	ifconfig
SRCS=	ifconfig.c brconfig.c
MAN=	ifconfig.8

LDADD=	-lutil
DPADD=	${LIBUTIL}

CPPFLAGS+=-DINET6 -I${.CURDIR}/../bioctl

.include <bsd.prog.mk>
