# $OpenBSD: Makefile,v 1.10 2013/06/19 19:36:46 robert Exp $

PROG=	hprop
MAN=	hprop.8
SRCS=	hprop.c mit_dump.c
CFLAGS+=-I${KRB5DIR}/lib/roken
CFLAGS+=-I${KRB5DIR}/lib/krb5
CFLAGS+=-I${KRB5DIR}/lib/asn1
CFLAGS+=-I${KRB5DIR}/lib/ntlm
CFLAGS+=-I${KRB5DIR}/kdc
CFLAGS+=-I${.OBJDIR}/../../lib/libkrb5
DPADD=	${LIBHDB} ${LIBKRB5} ${LIBHX509} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LDAPDPADD}
LDADD=	-lhdb -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcom_err -lheimbase -lwind

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kdc
