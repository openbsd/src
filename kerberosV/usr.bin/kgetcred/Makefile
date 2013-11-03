# $OpenBSD: Makefile,v 1.5 2013/11/03 11:56:40 ajacoutot Exp $

PROG=	kgetcred
MPAGES=	kgetcred.1
MSRC=	${KRB5DIR}/kuser
CPPFLAGS=	-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken
DPADD=	${LIBKRB5} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} ${LIBVERS}
LDADD=	-lkrb5 -lroken -lasn1 -lcrypto ${LIBVERS} -lheimbase -lcom_err \
	-lwind

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
