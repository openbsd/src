# $OpenBSD: Makefile,v 1.5 2007/09/05 07:12:35 grunk Exp $

PROG=	mount_vnd
SRCS=	mount_vnd.c pkcs5_pbkdf2.c
LDADD=	-lutil -lcrypto
DPADD=	${LIBUTIL}

LINKS=	${BINDIR}/mount_vnd ${BINDIR}/vnconfig
MLINKS=	mount_vnd.8 vnconfig.8

MAN=	mount_vnd.8

.include <bsd.prog.mk>
