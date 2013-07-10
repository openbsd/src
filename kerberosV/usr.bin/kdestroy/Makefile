# $OpenBSD: Makefile,v 1.13 2013/07/10 11:14:40 ajacoutot Exp $

PROG=	kdestroy
CPPFLAGS=	-I${KRB5DIR}/lib/roken
DPADD=	${LIBKAFS5} ${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO}
LDADD=	-lkafs5 -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lwind -lcom_err \
	-lheimbase
MPAGES=	kdestroy.1
MSRC=	${KRB5DIR}/kuser

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
