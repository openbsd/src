# $OpenBSD: Makefile,v 1.8 2012/09/06 19:41:59 tedu Exp $

PROG=	mount_vnd
SRCS=	mount_vnd.c
LDADD=	-lutil
DPADD=	${LIBUTIL}

CDIAGFLAGS+=	-Wall

LINKS=	${BINDIR}/mount_vnd ${BINDIR}/vnconfig
MLINKS=	mount_vnd.8 vnconfig.8

MAN=	mount_vnd.8

.include <bsd.prog.mk>
