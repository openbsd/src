# $OpenBSD: Makefile,v 1.4 2002/06/11 15:30:16 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	pagsh

.PATH:	${KRB5SRC}/appl/afsutil

CFLAGS+=-I${KRB5SRC}/lib/roken
DPADD+=	${LIBKRB5} ${LIBDES} ${LIBKAFS} ${LIBKRB} ${LIBASN1} ${LIBCRYPTO} \
	${LIBCOM_ERR}
LDADD+=	-lkrb5 -ldes -lkafs -lkrb ${LIBROKEN} -lasn1 -lcrypto -lcom_err

.include <bsd.prog.mk>
