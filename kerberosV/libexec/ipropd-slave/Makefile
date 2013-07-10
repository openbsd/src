# $OpenBSD: Makefile,v 1.15 2013/07/10 11:14:40 ajacoutot Exp $

PROG=	ipropd-slave
NOMAN=
SRCS=	ipropd_common.c ipropd_slave.c kadm5_err.h
CPPFLAGS=	-I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/roken \
		-I.
DPADD=	${LIBKADM5SRV} ${LIBHDB} ${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCOM_ERR}
LDADD=	-lkadm5srv -lhdb -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcom_err -lheimbase -lwind -lutil

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/lib/kadm5
