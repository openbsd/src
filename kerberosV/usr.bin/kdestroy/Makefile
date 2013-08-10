# $OpenBSD: Makefile,v 1.14 2013/08/10 09:07:41 ajacoutot Exp $

PROG=	kdestroy
CPPFLAGS=	-I${KRB5DIR}/lib/roken
DPADD=	${LIBKAFS5} ${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO}
LDADD=	-lkafs -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lwind -lcom_err \
	-lheimbase
MPAGES=	kdestroy.1
MSRC=	${KRB5DIR}/kuser

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
