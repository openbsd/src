# $OpenBSD: Makefile,v 1.6 2003/07/14 16:14:44 mho Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	pagsh

SRCS=	pagsh.c
SRCS+=	${PRINTVER}
SRCS+=	esetenv.c k_getpwuid.c

.PATH:	${KRB5SRC}/appl/afsutil
.PATH:	${ROKENSRC}

DPADD+=	${LIBKRB5} ${LIBDES} ${LIBCRYPTO}
LDADD+=	-lkrb5 -ldes -lcrypto

.include <bsd.prog.mk>
