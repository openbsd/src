# $OpenBSD: Makefile,v 1.2 2002/05/11 00:20:19 espie Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	string2key
MAN=	${PROG}.8

.PATH:	${KRB5SRC}/kdc

CFLAGS+=-I${KRB5SRC}/kdc -I${KRB5SRC}/lib/krb5
DPADD+=	${LIBKRB5} ${LIBDES} ${LIBKAFS} ${LIBKRB} ${LIBASN1} ${LIBCRYPTO} \
	${LIBCOM_ERR}
LDADD+=	-lkrb5 -ldes -lkafs -lkrb ${LIBROKEN} -lasn1 -lcrypto -lcom_err

.include <bsd.prog.mk>
