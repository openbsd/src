# $OpenBSD: Makefile,v 1.13 2013/06/19 19:36:49 robert Exp $

PROG=	kpasswdd
MAN=	kpasswdd.8
CFLAGS+=-I${KRB5DIR}/lib/roken -I../../lib/libhdb ${LDAPCFLAGS}
DPADD=	${LIBKADM5SRV} ${LIBHDB} ${LIBKRB5} ${LIBHX509} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LDAPDPADD}
LDADD=	-lkadm5srv -lhdb -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcom_err -lheimbase -lwind -lutil

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kpasswd
