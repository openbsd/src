# $OpenBSD: Makefile,v 1.11 2013/07/10 10:43:32 ajacoutot Exp $

PROG=	kfd
MPAGES=	kfd.8
MSRC=	${KRB5DIR}/appl/kf

CPPFLAGS=	-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken
DPADD=	${LIBKRB5} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} \
	${LIBVERS}
LDADD=	-lkrb5 -lroken -lasn1 -lcrypto \
	${LIBVERS} -lcom_err \
	-lwind -lheimbase

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/appl/kf
