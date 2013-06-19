# $OpenBSD: Makefile,v 1.8 2013/06/19 19:36:49 robert Exp $

PROG=	kfd
MAN=	kfd.8
CFLAGS+=	-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken
DPADD=	${LIBKRB5} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} \
	${LIBCRYPT} ${LIBVERS}
LDADD=	-lkrb5 -lroken -lasn1 -lcrypto \
	${LIBVERS} -lcom_err \
	-lwind -lheimbase

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/appl/kf
