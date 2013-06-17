# $OpenBSD: Makefile,v 1.10 2013/06/17 18:57:46 robert Exp $

PROG=	kstash
MAN=	kstash.8
CFLAGS+=-I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/roken \
	-I${KRB5DIR}/kdc ${LDAPCFLAGS}
DPADD=	${LIBHDB} ${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LDAPDPADD}
LDADD=	-lhdb -lkrb5 -lroken ${LIBVERS} -lpthread \
	-lasn1 -lcrypto -lhx509 -lcom_err -lheimbase -lwind

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kdc
