# $OpenBSD: Makefile,v 1.6 2003/07/14 16:14:44 mho Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	klist

SRCS=	klist.c
SRCS+=	${PRINTVER}
SRCS+=	rtbl.c

.PATH:	${KRB5SRC}/kuser
.PATH:	${ROKENSRC}

DPADD+=	${LIBKRB5} ${LIBDES} ${LIBCRYPTO}
LDADD+=	-lkrb5 -ldes -lcrypto

.include <bsd.prog.mk>
