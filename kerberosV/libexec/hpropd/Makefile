# $OpenBSD: Makefile,v 1.14 2013/07/10 11:14:39 ajacoutot Exp $

PROG=	hpropd
MPAGES=	hpropd.8
MSRC=	${KRB5DIR}/kdc
CPPFLAGS=	-I${KRB5DIR}/lib/roken -I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/kdc
DPADD=	${LIBHDB} ${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCOM_ERR}
LDADD=	-lhdb -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcom_err -lheimbase -lwind

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kdc
