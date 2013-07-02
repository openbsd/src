# $OpenBSD: Makefile,v 1.13 2013/07/02 19:00:10 ajacoutot Exp $

PROG=	ipropd-master
MPAGES=	iprop.8
MSRC=	${KRB5DIR}/lib/kadm5
SRCS=	ipropd_common.c ipropd_master.c kadm5_err.h
CPPFLAGS=	-I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/roken \
		-I.
DPADD=	${LIBKADM5SRV} ${LIBHDB} ${LIBKRB5} ${LIBHX509} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR}
LDADD=	-lkadm5srv -lhdb -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcom_err -lwind -lheimbase \
	-lutil

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/lib/kadm5
