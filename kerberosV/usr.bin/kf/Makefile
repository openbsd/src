# $OpenBSD: Makefile,v 1.5 2013/06/17 18:57:45 robert Exp $

PROG=	kf
MAN=	kf.1
CFLAGS+=	-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken
DPADD=	${LIBKRB5} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} \
	${LIBCRYPT} ${LIBVERS}
LDADD=	-lkrb5 -lroken -lasn1 -lcrypto -lpthread \
	${LIBVERS} -lhx509 -lheimbase -lwind -lcom_err

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/appl/kf
