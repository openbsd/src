# $OpenBSD: Makefile,v 1.4 2002/02/06 09:13:34 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	ipropd-slave
MAN=	${PROG}.8

SRCS=	ipropd_slave.c
NOMAN=

.PATH:	${KRB5SRC}/lib/kadm5
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
LDADD+=	-lhdb -lkadm5srv -lkrb5 -lkrb -lkdb ${LIBROKEN} -lasn1 -lcrypto \
	-lcom_err -lhdb -lkafs -lutil

.include <bsd.prog.mk>
