# $OpenBSD: Makefile,v 1.14 2013/06/17 18:57:39 robert Exp $

PROG=	kdc
MAN=	kdc.8

SRCS=	config.c \
	connect.c \
	announce.c \
	main.c

CFLAGS+=-I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/roken \
	-I${KRB5DIR}/kdc ${LDAPCFLAGS}
DPADD=	${LIBKDC} ${LIBHDB} ${LIBKRB5} ${LIBROKEN} ${LIBASN1} \
	${LIBCRYPTO} ${LIBCRYPT} ${LIBVERS}
LDADD=	-lkdc -lhdb -lkrb5 -lroken -lasn1 -lcrypto ${LIBVERS} -lhx509 -lheimbase \
	-lwind -lcom_err -lheimntlm -lutil -lpthread

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kdc
