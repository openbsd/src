# $OpenBSD: Makefile,v 1.8 2003/07/14 16:14:44 mho Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kinit
MLINKS=	kinit.1 kauth.1

SRCS=	kinit.c
SRCS+=	${PRINTVER}

.PATH:	${KRB5SRC}/kuser

DPADD+=	${LIBKRB5} ${LIBDES} ${LIBCRYPTO}
LDADD+=	-lkrb5 -ldes -lcrypto

.include <bsd.prog.mk>
