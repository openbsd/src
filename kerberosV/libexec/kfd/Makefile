# $OpenBSD: Makefile,v 1.7 2013/06/17 18:57:39 robert Exp $

PROG=	kfd
MAN=	kfd.8
CFLAGS+=	-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken
DPADD=	${LIBKRB5} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} \
	${LIBCRYPT} ${LIBVERS}
LDADD=	-lkrb5 -lroken -lasn1 -lcrypto \
	${LIBVERS} -lhx509 -lcom_err -lpthread \
	-lwind -lheimbase

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/appl/kf
