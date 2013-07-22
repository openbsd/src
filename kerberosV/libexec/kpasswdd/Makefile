# $OpenBSD: Makefile,v 1.18 2013/07/22 13:20:48 ajacoutot Exp $

PROG=	kpasswdd
MPAGES=	kpasswdd.8
MSRC=	${KRB5DIR}/kpasswd

CPPFLAGS=	-I${KRB5DIR}/lib/roken -I${KRB5DIR}/lib/hdb
DPADD=	${LIBKADM5SRV} ${LIBHDB} ${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCOM_ERR}
LDADD=	-lkadm5srv -lhdb -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcom_err -lheimbase -lwind -lutil

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kpasswd
