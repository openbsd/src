# $OpenBSD: Makefile,v 1.6 2003/05/17 01:31:21 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	hpropd
MAN=	${PROG}.8

SRCS=	hpropd.c

.PATH:	${KRB5SRC}/kdc
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
DPADD+= ${LIBHDB} ${LIBKADM5SRV} ${LIBKRB5} ${LIBKDB} ${LIBKRB} ${LIBASN1} \
	${LIBCRYPTO} ${LIBCOM_ERR} ${LIBKAFS} ${LIBDES}
LDADD+=	-lhdb -lkadm5srv -lkrb5 ${LIBROKEN} -lasn1 -lcrypto \
	-lcom_err -ldes

.include <bsd.prog.mk>
