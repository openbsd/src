# $OpenBSD: Makefile,v 1.2 2013/06/19 19:36:55 robert Exp $

PROG=	kgetcred
CFLAGS+=	-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken
DPADD=	${LIBKRB5} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBVERS}
LDADD=	-lkrb5 -lroken -lasn1 -lcrypto ${LIBVERS} -lheimbase -lcom_err \
	-lwind

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
