# $OpenBSD: Makefile,v 1.6 2003/05/14 06:53:34 mho Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kauth
NOMAN=

SRCS=	kinit.c

.PATH:	${KRB5SRC}/kuser

DPADD+= ${LIBKRB5} ${LIBDES} ${LIBASN1} ${LIBCRYPTO} ${LIBCOM_ERR}
LDADD+=	-lkrb5 -ldes ${LIBROKEN} -lasn1 -lcrypto -lcom_err

.include <bsd.prog.mk>
