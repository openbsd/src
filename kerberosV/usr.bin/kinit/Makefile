# $OpenBSD: Makefile,v 1.7 2003/05/14 06:56:02 mho Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kinit
MLINKS=	kinit.1 kauth.1

.PATH:	${KRB5SRC}/kuser

DPADD+=	${LIBKRB5} ${LIBDES} ${LIBASN1} ${LIBCRYPTO} ${LIBCOM_ERR}
LDADD+=	-lkrb5 -ldes ${LIBROKEN} -lasn1 -lcrypto -lcom_err

.include <bsd.prog.mk>
