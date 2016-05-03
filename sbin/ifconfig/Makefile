#	$OpenBSD: Makefile,v 1.14 2016/05/03 17:52:33 jca Exp $

PROG=	ifconfig
SRCS=	ifconfig.c brconfig.c
MAN=	ifconfig.8

LDADD=	-lutil
DPADD=	${LIBUTIL}

.include <bsd.prog.mk>
