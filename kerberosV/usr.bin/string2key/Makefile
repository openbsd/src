# $OpenBSD: Makefile,v 1.6 2013/06/17 18:57:46 robert Exp $

PROG=	string2key
MAN=	string2key.8
CFLAGS+=	-I${KRB5DIR}/kdc \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/windc
DPADD=	${LIBHDB} ${LIBKRB5} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} \
	${LIBCRYPT} ${LIBVERS}
LDADD=	-lhdb -lkrb5 -lroken -lasn1 -lcrypto ${LIBVERS} -lhx509 -lwind \
	-lheimbase -lcom_err -lpthread

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kdc
