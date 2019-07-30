# $OpenBSD: Makefile,v 1.10 2017/07/09 21:23:19 espie Exp $

PROG=	mount_vnd
LDADD=	-lutil
DPADD=	${LIBUTIL}

CDIAGFLAGS+=	-Wall

LINKS=	${BINDIR}/mount_vnd ${BINDIR}/vnconfig

MAN=	mount_vnd.8

.include <bsd.prog.mk>
