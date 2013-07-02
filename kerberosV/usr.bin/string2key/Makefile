# $OpenBSD: Makefile,v 1.9 2013/07/02 19:00:10 ajacoutot Exp $

PROG=	string2key
MPAGES=	string2key.8
MSRC=	${KRB5DIR}/kdc
CPPFLAGS=	-I${KRB5DIR}/kdc \
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
