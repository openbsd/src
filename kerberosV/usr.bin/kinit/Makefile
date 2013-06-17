# $OpenBSD: Makefile,v 1.10 2013/06/17 18:57:45 robert Exp $

PROG=	kinit
CFLAGS+=-I${KRB5DIR}/lib/roken
DPADD=	${LIBKAFS5} ${LIBKRB5} ${LIBHEIMNTLM} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT}
LDADD=	-lkafs5 -lkrb5 -lheimntlm -lroken ${LIBVERS} -lpthread \
	-lasn1 -lcrypto -lcom_err -lhx509 -lheimbase -lwind

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
