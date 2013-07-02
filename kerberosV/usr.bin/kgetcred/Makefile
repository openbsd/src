# $OpenBSD: Makefile,v 1.3 2013/07/02 19:00:10 ajacoutot Exp $

PROG=	kgetcred
CPPFLAGS=	-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken
DPADD=	${LIBKRB5} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBVERS}
LDADD=	-lkrb5 -lroken -lasn1 -lcrypto ${LIBVERS} -lheimbase -lcom_err \
	-lwind

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
