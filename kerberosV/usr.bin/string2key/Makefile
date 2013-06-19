# $OpenBSD: Makefile,v 1.7 2013/06/19 19:36:56 robert Exp $

PROG=	string2key
MAN=	string2key.8
CFLAGS+=	-I${KRB5DIR}/kdc \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/windc
DPADD=	${LIBHDB} ${LIBKRB5} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} \
	${LIBCRYPT} ${LIBVERS}
LDADD=	-lhdb -lkrb5 -lroken -lasn1 -lcrypto ${LIBVERS} -lwind \
	-lheimbase -lcom_err

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kdc
