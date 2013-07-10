# $OpenBSD: Makefile,v 1.10 2013/07/10 10:43:32 ajacoutot Exp $

PROG=	string2key
MPAGES=	string2key.8
MSRC=	${KRB5DIR}/kdc
CPPFLAGS=	-I${KRB5DIR}/kdc \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/windc
DPADD=	${LIBHDB} ${LIBKRB5} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} \
	${LIBVERS}
LDADD=	-lhdb -lkrb5 -lroken -lasn1 -lcrypto ${LIBVERS} -lwind \
	-lheimbase -lcom_err

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kdc
