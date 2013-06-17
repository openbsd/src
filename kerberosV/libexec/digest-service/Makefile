# $OpenBSD: Makefile,v 1.1 2013/06/17 19:11:35 robert Exp $

PROG=	digest-service
NOMAN=	1
CFLAGS+=	-I${KRB5DIR}/kdc \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/ipc \
		-I${KRB5DIR}/lib/wind \
		-I${KRB5DIR}/lib/roken
DPADD=	${LIBHDB} ${LIBKDC} ${LIBHEIMIPCS} ${LIBKRB5} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} \
	${LIBCRYPT} ${LIBVERS}
LDADD=	-lhdb -lkdc ${LIBHEIMIPCS} -lkrb5 -lroken -lasn1 -lcrypto \
	${LIBVERS} -lheimntlm -lcom_err -lhx509 \
	-lheimbase -lwind -lpthread

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kdc
