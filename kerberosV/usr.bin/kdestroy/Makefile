# $OpenBSD: Makefile,v 1.9 2013/06/19 19:36:54 robert Exp $

PROG=	kdestroy
CFLAGS+=-I${KRB5DIR}/lib/roken
DPADD=	${LIBKAFS5} ${LIBKRB5} ${LIBHEIMNTLM} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT}
LDADD=	-lkafs5 -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lwind -lcom_err \
	-lheimbase
MAN=	kdestroy.1

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
