# $OpenBSD: Makefile,v 1.2 2018/12/06 16:51:19 tedu Exp $

PROG=	rebound
SRCS=	rebound.c randomid.c https.c
LDADD=	-ltls
DPADD=	${LIBTLS}

CFLAGS+=-Wall

MAN=	rebound.8

.include <bsd.prog.mk>
