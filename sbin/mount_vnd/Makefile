# $OpenBSD: Makefile,v 1.9 2016/03/30 06:38:43 jmc Exp $

PROG=	mount_vnd
SRCS=	mount_vnd.c
LDADD=	-lutil
DPADD=	${LIBUTIL}

CDIAGFLAGS+=	-Wall

LINKS=	${BINDIR}/mount_vnd ${BINDIR}/vnconfig

MAN=	mount_vnd.8

.include <bsd.prog.mk>
