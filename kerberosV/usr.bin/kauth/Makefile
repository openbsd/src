# $OpenBSD: Makefile,v 1.8 2010/10/13 08:14:22 jsg Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kauth
NOMAN=

SRCS=	kinit.c
SRCS+=	${PRINTVER}

.PATH:	${KRB5SRC}/kuser

DPADD+= ${LIBKRB5} ${LIBCRYPTO}
LDADD+=	-lkrb5 -lcrypto

.include <bsd.prog.mk>
