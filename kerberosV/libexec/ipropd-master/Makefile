# $OpenBSD: Makefile,v 1.5 2002/05/11 00:20:19 espie Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	ipropd-master
MAN=	${PROG}.8

SRCS=	ipropd_master.c
NOMAN=

.PATH:	${KRB5SRC}/lib/kadm5
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
DPADD+= ${LIBHDB} ${LIBKADM5SRV} ${LIBKRB5} ${LIBKRB} ${LIBKDB} ${LIBASN1} \
	${LIBCRYPTO} ${LIBCOM_ERR} ${LIBKAFS} ${LIBUTIL} ${LIBDES}
LDADD+=	-lhdb -lkadm5srv -lkrb5 -lkrb -lkdb ${LIBROKEN} -lasn1 -lcrypto \
	-lcom_err -lhdb -lkafs -lutil -ldes

.include <bsd.prog.mk>
