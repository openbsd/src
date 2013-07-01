# $OpenBSD: Makefile,v 1.12 2013/07/01 05:15:24 ajacoutot Exp $

PROG=	kstash
MPAGES=	kstash.8
MSRC=	${KRB5DIR}/kdc

CFLAGS+=-I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/roken \
	-I${KRB5DIR}/kdc ${LDAPCFLAGS}
DPADD=	${LIBHDB} ${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LDAPDPADD}
LDADD=	-lhdb -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcom_err -lheimbase -lwind

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kdc
