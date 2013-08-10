# $OpenBSD: Makefile,v 1.16 2013/08/10 14:54:03 ajacoutot Exp $

PROG=	kinit
CPPFLAGS=	-I${KRB5DIR}/lib/roken
DPADD=	${LIBKAFS} ${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO}
LDADD=	-lkafs -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcom_err -lheimbase -lwind

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
