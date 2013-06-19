# $OpenBSD: Makefile,v 1.10 2013/06/19 19:36:46 robert Exp $

PROG=	hpropd
MAN=	hpropd.8
CFLAGS+=-I${KRB5DIR}/lib/roken -I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/asn1 \
	-I${KRB5DIR}/kdc ${LDAPCFLAGS}
DPADD=	${LIBHDB} ${LIBKRB5} ${LIBHX509} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LDAPDPADD}
LDADD=	-lhdb -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcom_err -lheimbase -lwind
LDFLAGS=${LDAPLDFLAGS}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kdc
