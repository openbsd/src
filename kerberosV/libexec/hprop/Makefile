# $OpenBSD: Makefile,v 1.6 2003/05/17 01:31:21 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	hprop
MAN=	${PROG}.8

SRCS=	hprop.c mit_dump.c v4_dump.c hprop.h kadb.h

.PATH:	${KRB5SRC}/kdc
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
DPADD+= ${LIBHDB} ${LIBKADM5SRV} ${LIBKDB} ${LIBKRB} ${LIBKDB} ${LIBASN1} \
	${LIBCOM_ERR} ${LIBKAFS} ${LIBDES} ${LIBCRYPTO}
LDADD+=	-lhdb -lkadm5srv -lkrb5 ${LIBROKEN} -lasn1 \
	-lcom_err -ldes -lcrypto

.include <bsd.prog.mk>
