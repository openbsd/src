# $OpenBSD: Makefile,v 1.1 2001/06/25 02:29:56 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	ipropd-master
MAN=	${PROG}.8

SRCS=	ipropd_slave.c
NOMAN=

.PATH:	${KRB5SRC}/lib/kadm5
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
LDADD+=	-lhdb -lkadm5srv -lkrb5 -lkrb -lkdb ${LIBROKEN} -lasn1 -lcrypto \
	-lcom_err -lhdb

.include <bsd.prog.mk>
