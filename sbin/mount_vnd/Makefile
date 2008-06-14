# $OpenBSD: Makefile,v 1.6 2008/06/14 01:47:27 grunk Exp $

PROG=	mount_vnd
SRCS=	mount_vnd.c pkcs5_pbkdf2.c
LDADD=	-lutil -lcrypto
DPADD=	${LIBUTIL}

CDIAGFLAGS+=	-Wall

LINKS=	${BINDIR}/mount_vnd ${BINDIR}/vnconfig
MLINKS=	mount_vnd.8 vnconfig.8

MAN=	mount_vnd.8

.include <bsd.prog.mk>
