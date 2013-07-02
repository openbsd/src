# $OpenBSD: Makefile,v 1.12 2013/07/02 19:00:10 ajacoutot Exp $

PROG=	hprop
MPAGES=	hprop.8
MSRC=	${KRB5DIR}/kdc
SRCS=	hprop.c mit_dump.c
CPPFLAGS=	-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/ntlm \
		-I${KRB5DIR}/kdc \
		-I${.OBJDIR}/../../lib/libkrb5
DPADD=	${LIBHDB} ${LIBKRB5} ${LIBHX509} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR}
LDADD=	-lhdb -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcom_err -lheimbase -lwind

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kdc
