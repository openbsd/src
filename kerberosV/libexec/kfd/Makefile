# $OpenBSD: Makefile,v 1.5 2003/07/14 16:14:44 mho Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kfd
MAN=	${PROG}.8

SRCS=	kfd.c

.PATH:	${KRB5SRC}/appl/kf

CFLAGS+=-I${KRB5SRC}/lib/krb5

DPADD+=	${LIBKRB5} ${LIBDES} ${LIBCRYPTO}
LDADD+=	-lkrb5 -ldes -lcrypto

.include <bsd.prog.mk>
