# $OpenBSD: Makefile,v 1.8 2002/05/11 00:20:19 espie Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kpasswdd
MAN=	${PROG}.8

.PATH:	${KRB5SRC}/kpasswd
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
DPADD+=	${LIBHDB} ${LIBKADM5SRV} ${LIBKRB5} ${LIBKRB} ${LIBDES} ${LIBASN1} \
	${LIBCRYPTO} ${LIBCOM_ERR} ${LIBKAFS} ${LIBUTIL}
LDADD+=	-lhdb -lkadm5srv -lkrb5 -lkrb -ldes ${LIBROKEN} -lasn1 -lcrypto \
	-lcom_err -lkafs -lutil

.include <bsd.prog.mk>
