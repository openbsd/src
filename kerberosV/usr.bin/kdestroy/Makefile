# $OpenBSD: Makefile,v 1.11 2013/07/02 19:00:10 ajacoutot Exp $

PROG=	kdestroy
CPPFLAGS=	-I${KRB5DIR}/lib/roken
DPADD=	${LIBKAFS5} ${LIBKRB5} ${LIBHEIMNTLM} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT}
LDADD=	-lkafs5 -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lwind -lcom_err \
	-lheimbase
MPAGES=	kdestroy.1
MSRC=	${KRB5DIR}/kuser

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
