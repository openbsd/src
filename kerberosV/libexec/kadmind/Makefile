# $OpenBSD: Makefile,v 1.18 2013/07/02 19:00:10 ajacoutot Exp $

PROG=	kadmind
MPAGES=	kadmind.8
MSRC=	${KRB5DIR}/kadmin
SRCS=	rpc.c \
	server.c \
	kadmind.c \
	kadm_conn.c

CPPFLAGS=	-I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/roken \
		-I/usr/include/kerberosV/gssapi
DPADD=	${LIBKADM5SRV} ${LIBGSSAPI} ${LIBHDB} ${LIBKRB5} ${LIBROKEN} \
	${LIBASN1} ${LIBVERS} ${LIBCRYPTO} ${LIBCRYPT}
LDADD=	-lkadm5srv -lgssapi -lhdb -lkrb5 -lasn1 \
	-lroken ${LIBVERS} -lcrypto  -lheimbase -lwind \
	-lcom_err -lutil

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kadmin
