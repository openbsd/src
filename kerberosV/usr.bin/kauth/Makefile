# $OpenBSD: Makefile,v 1.5 2002/05/11 00:20:19 espie Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kauth
NOMAN=

SRCS=	kinit.c

.PATH:	${KRB5SRC}/kuser

DPADD+= ${LIBKRB5} ${LIBDES} ${LIBKAFS} ${LIB45} ${LIBKRB} ${LIBASN1} \
	${LIBCRYPTO} ${LIBCOM_ERR}
LDADD+=	-lkrb5 -ldes -lkafs -l45 -lkrb ${LIBROKEN} -lasn1 -lcrypto -lcom_err

.include <bsd.prog.mk>
