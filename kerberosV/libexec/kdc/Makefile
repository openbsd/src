# $OpenBSD: Makefile,v 1.16 2013/07/01 05:15:24 ajacoutot Exp $

PROG=	kdc
MPAGES=	kdc.8
MSRC=	${KRB5DIR}/kdc

SRCS=	config.c \
	connect.c \
	announce.c \
	main.c

CFLAGS+=-I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/roken \
	-I${KRB5DIR}/kdc ${LDAPCFLAGS}
DPADD=	${LIBKDC} ${LIBHDB} ${LIBKRB5} ${LIBROKEN} ${LIBASN1} \
	${LIBCRYPTO} ${LIBCRYPT} ${LIBVERS}
LDADD=	-lkdc -lhdb -lkrb5 -lroken -lasn1 -lcrypto ${LIBVERS} -lheimbase \
	-lwind -lcom_err -lutil

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kdc
