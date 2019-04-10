#	$OpenBSD: Makefile,v 1.15 2019/04/10 10:14:37 dlg Exp $

PROG=	ifconfig
SRCS=	ifconfig.c brconfig.c sff.c
MAN=	ifconfig.8

LDADD=	-lutil -lm
DPADD=	${LIBUTIL}

.include <bsd.prog.mk>
