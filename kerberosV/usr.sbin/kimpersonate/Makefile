# $OpenBSD: Makefile,v 1.8 2013/08/10 14:54:04 ajacoutot Exp $

PROG=	kimpersonate
MPAGES=	kimpersonate.8
MSRC=	${KRB5DIR}/kuser

CPPFLAGS=	-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/sl -I.
DPADD=	${LIBKAFS} ${LIBKRB5} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} \
	${LIBVERS}
LDADD=	-lkafs -lkrb5 -lroken -lasn1 -lcrypto \
	${LIBVERS} -lcom_err -lheimbase -lwind

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
