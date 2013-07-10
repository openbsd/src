# $OpenBSD: Makefile,v 1.4 2013/07/10 10:43:32 ajacoutot Exp $

PROG=	kgetcred
CPPFLAGS=	-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken
DPADD=	${LIBKRB5} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} ${LIBVERS}
LDADD=	-lkrb5 -lroken -lasn1 -lcrypto ${LIBVERS} -lheimbase -lcom_err \
	-lwind

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
