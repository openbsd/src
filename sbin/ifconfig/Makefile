#	$OpenBSD: Makefile,v 1.16 2020/06/21 12:20:06 dlg Exp $

PROG=	ifconfig
SRCS=	ifconfig.c brconfig.c sff.c
MAN=	ifconfig.8

LDADD=	-lutil -lm -lcrypto
DPADD=	${LIBUTIL}

.include <bsd.prog.mk>
