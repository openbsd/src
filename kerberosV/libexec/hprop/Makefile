# $OpenBSD: Makefile,v 1.16 2013/08/09 18:29:57 ajacoutot Exp $

PROG=	hprop
MPAGES=	hprop.8
MSRC=	${KRB5DIR}/kdc
SRCS=	hprop.c mit_dump.c
CPPFLAGS=	-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/ntlm \
		-I${KRB5DIR}/kdc
DPADD=	${LIBHDB} ${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCOM_ERR}
LDADD=	-lhdb -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcom_err -lheimbase -lwind

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kdc
