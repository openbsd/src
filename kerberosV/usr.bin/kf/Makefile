# $OpenBSD: Makefile,v 1.9 2013/07/10 10:43:32 ajacoutot Exp $

PROG=	kf
MPAGES=	kf.1
MSRC=	${KRB5DIR}/appl/kf
CPPFLAGS=	-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken
DPADD=	${LIBKRB5} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} \
	${LIBVERS}
LDADD=	-lkrb5 -lroken -lasn1 -lcrypto \
	${LIBVERS} -lheimbase -lwind -lcom_err

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/appl/kf
