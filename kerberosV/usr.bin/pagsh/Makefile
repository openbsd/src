# $OpenBSD: Makefile,v 1.7 2010/10/13 08:14:22 jsg Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	pagsh

SRCS=	pagsh.c
SRCS+=	${PRINTVER}
SRCS+=	esetenv.c k_getpwuid.c

.PATH:	${KRB5SRC}/appl/afsutil
.PATH:	${ROKENSRC}

DPADD+=	${LIBKRB5} ${LIBCRYPTO}
LDADD+=	-lkrb5 -lcrypto

.include <bsd.prog.mk>
