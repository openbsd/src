# $OpenBSD: Makefile,v 1.17 2013/11/03 11:56:40 ajacoutot Exp $

PROG=	kinit
MPAGES=	kinit.1
MSRC=	${KRB5DIR}/kuser
CPPFLAGS=	-I${KRB5DIR}/lib/roken
DPADD=	${LIBKAFS} ${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO}
LDADD=	-lkafs -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcom_err -lheimbase -lwind

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
