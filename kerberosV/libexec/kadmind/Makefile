# $OpenBSD: Makefile,v 1.10 2002/05/11 00:20:19 espie Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kadmind
MAN=	${PROG}.8

SRCS=	kadmind.c server.c version4.c kadm_conn.c

.PATH:	${KRB5SRC}/kadmin
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
DPADD+=	${LIBHDB} ${LIBKADM5SRV} ${LIBKRB5} ${LIBKRB} ${LIBDES} ${LIBASN1} \
	${LIBCRYPTO} ${LIBCOM_ERR} ${LIBKAFS} ${LIBUTIL}
LDADD+=	-lhdb -lkadm5srv -lkrb5 -lkrb -ldes ${LIBROKEN} -lasn1 -lcrypto \
	-lcom_err -lkafs -lutil

.include <bsd.prog.mk>
