# $OpenBSD: Makefile,v 1.5 2013/07/10 10:43:32 ajacoutot Exp $

PROG=	kimpersonate
MPAGES=	kimpersonate.8
MSRC=	${KRB5DIR}/kuser

CPPFLAGS=	-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/sl -I.
DPADD=	${LIBKAFS5} ${LIBKRB5} ${LIBHEIMNTLM} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} \
	${LIBVERS}
LDADD=	-lkafs5 -lkrb5 -lroken -lasn1 -lcrypto \
	${LIBVERS} -lcom_err -lheimbase -lwind

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
