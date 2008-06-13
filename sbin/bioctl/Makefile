#	$OpenBSD: Makefile,v 1.9 2008/06/13 21:03:40 hshoexer Exp $

PROG=	bioctl
SRCS=	bioctl.c pkcs5_pbkdf2.c
.PATH:	${.CURDIR}/../mount_vnd
CFLAGS+=-Wall -I${.CURDIR}/../mount_vnd
LDADD=	-lutil -lcrypto
DPADD=	${LIBUTIL}
MAN=	bioctl.8

.include <bsd.prog.mk>
