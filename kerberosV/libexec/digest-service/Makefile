# $OpenBSD: Makefile,v 1.4 2013/07/10 10:43:32 ajacoutot Exp $

PROG=	digest-service
NOMAN=	1
CPPFLAGS=	-I${KRB5DIR}/kdc \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/ipc \
		-I${KRB5DIR}/lib/wind \
		-I${KRB5DIR}/lib/roken
DPADD=	${LIBHDB} ${LIBKDC} ${LIBHEIMIPCS} ${LIBKRB5} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} \
	${LIBVERS}
LDADD=	-lhdb -lkdc ${LIBHEIMIPCS} -lkrb5 -lroken -lasn1 -lcrypto \
	${LIBVERS} -lheimntlm -lcom_err \
	-lheimbase -lwind

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kdc
