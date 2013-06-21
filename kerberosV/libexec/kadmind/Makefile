# $OpenBSD: Makefile,v 1.16 2013/06/21 20:34:23 miod Exp $

PROG=	kadmind
MAN=	kadmind.8
SRCS=	rpc.c \
	server.c \
	kadmind.c \
	kadm_conn.c

CFLAGS+=-I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/roken \
	-I/usr/include/kerberosV/gssapi
DPADD=	${LIBKADM5SRV} ${LIBGSSAPI} ${LIBHDB} ${LIBKRB5} ${LIBROKEN} \
	${LIBASN1} ${LIBVERS} ${LIBCRYPTO} ${LIBCRYPT} ${LDAPDPADD}
LDADD=	-lkadm5srv -lgssapi -lhdb -lkrb5 -lasn1 \
	-lroken ${LIBVERS} -lcrypto  -lheimbase -lwind \
	-lcom_err -lutil

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kadmin
