# $OpenBSD: Makefile,v 1.15 2013/08/10 14:54:03 ajacoutot Exp $

PROG=	kdestroy
CPPFLAGS=	-I${KRB5DIR}/lib/roken
DPADD=	${LIBKAFS} ${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO}
LDADD=	-lkafs -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lwind -lcom_err \
	-lheimbase
MPAGES=	kdestroy.1
MSRC=	${KRB5DIR}/kuser

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
