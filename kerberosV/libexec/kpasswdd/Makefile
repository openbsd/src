# $OpenBSD: Makefile,v 1.15 2013/07/02 19:00:10 ajacoutot Exp $

PROG=	kpasswdd
MPAGES=	kpasswdd.8
MSRC=	${KRB5DIR}/kpasswd

CPPFLAGS=	-I${KRB5DIR}/lib/roken -I../../lib/libhdb
DPADD=	${LIBKADM5SRV} ${LIBHDB} ${LIBKRB5} ${LIBHX509} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR}
LDADD=	-lkadm5srv -lhdb -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcom_err -lheimbase -lwind -lutil

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kpasswd
