# $OpenBSD: Makefile,v 1.4 2002/05/11 00:20:19 espie Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kdestroy

SRCS=	kdestroy.c

.PATH:	${KRB5SRC}/kuser

DPADD+= ${LIBKRB5} ${LIBDES} ${LIBKAFS} ${LIBKRB} ${LIBASN1} ${LIBCRYPTO} \
	${LIBCOM_ERR}
LDADD+=	-lkrb5 -ldes -lkafs -lkrb -lasn1 -lcrypto ${LIBROKEN} -lcom_err

.include <bsd.prog.mk>
