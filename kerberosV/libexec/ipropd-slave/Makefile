# $OpenBSD: Makefile,v 1.7 2003/05/17 01:31:21 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	ipropd-slave
MAN=	${PROG}.8

SRCS=	ipropd_slave.c
NOMAN=

.PATH:	${KRB5SRC}/lib/kadm5
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
DPADD+= ${LIBHDB} ${LIBKADM5SRV} ${LIBKRB5} ${LIBKRB} ${LIBKDB} ${LIBASN1} \
	${LIBCRYPTO} ${LIBCOM_ERR} ${LIBKAFS} ${LIBUTIL} ${LIBDES}
LDADD+=	-lhdb -lkadm5srv -lkrb5 ${LIBROKEN} -lasn1 -lcrypto \
	-lcom_err -lhdb -lutil -ldes

.include <bsd.prog.mk>
