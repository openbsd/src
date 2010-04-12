# $OpenBSD: Makefile,v 1.7 2010/04/12 01:44:08 tedu Exp $

.PATH: ${.CURDIR}/../bioctl
CFLAGS+=-I${.CURDIR}/../bioctl

PROG=	mount_vnd
SRCS=	mount_vnd.c pbkdf2.c
LDADD=	-lutil
DPADD=	${LIBUTIL}

CDIAGFLAGS+=	-Wall

LINKS=	${BINDIR}/mount_vnd ${BINDIR}/vnconfig
MLINKS=	mount_vnd.8 vnconfig.8

MAN=	mount_vnd.8

.include <bsd.prog.mk>
