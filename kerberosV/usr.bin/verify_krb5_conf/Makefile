# $OpenBSD: Makefile,v 1.2 2002/05/11 00:20:19 espie Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	verify_krb5_conf

.PATH:	${KRB5SRC}/lib/krb5
MAN=	${PROG}.8

CFLAGS+=-I${KRB5SRC}/lib/roken
DPADD+= ${LIBKRB5} ${LIBDES} ${LIBKAFS} ${LIBKRB} ${LIBASN1} ${LIBCRYPTO} \
	${LIBCOM_ERR}
LDADD+=	-lkrb5 -ldes -lkafs -lkrb ${LIBROKEN} -lasn1 -lcrypto -lcom_err

.include <bsd.prog.mk>
