# $OpenBSD: Makefile,v 1.14 2013/07/01 05:15:24 ajacoutot Exp $

PROG=	kpasswdd
MPAGES=	kpasswdd.8
MSRC=	${KRB5DIR}/kpasswd

CFLAGS+=-I${KRB5DIR}/lib/roken -I../../lib/libhdb ${LDAPCFLAGS}
DPADD=	${LIBKADM5SRV} ${LIBHDB} ${LIBKRB5} ${LIBHX509} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LDAPDPADD}
LDADD=	-lkadm5srv -lhdb -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcom_err -lheimbase -lwind -lutil

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kpasswd
