# $OpenBSD: Makefile,v 1.5 2002/05/11 00:20:19 espie Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	hpropd
MAN=	${PROG}.8

SRCS=	hpropd.c

.PATH:	${KRB5SRC}/kdc
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
DPADD+= ${LIBHDB} ${LIBKADM5SRV} ${LIBKRB5} ${LIBKDB} ${LIBKRB} ${LIBASN1} \
	${LIBCRYPTO} ${LIBCOM_ERR} ${LIBKAFS} ${LIBDES}
LDADD+=	-lhdb -lkadm5srv -lkrb5 -lkdb -lkrb -lkdb ${LIBROKEN} -lasn1 -lcrypto \
	-lcom_err -lkafs -ldes

.include <bsd.prog.mk>
