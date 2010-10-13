# $OpenBSD: Makefile,v 1.7 2010/10/13 08:14:22 jsg Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	klist

SRCS=	klist.c
SRCS+=	${PRINTVER}
SRCS+=	rtbl.c

.PATH:	${KRB5SRC}/kuser
.PATH:	${ROKENSRC}

DPADD+=	${LIBKRB5} ${LIBCRYPTO}
LDADD+=	-lkrb5 -lcrypto

.include <bsd.prog.mk>
