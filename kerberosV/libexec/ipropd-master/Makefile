# $OpenBSD: Makefile,v 1.2 2001/07/18 20:24:18 deraadt Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	ipropd-master
MAN=	${PROG}.8

SRCS=	ipropd_master.c
NOMAN=

.PATH:	${KRB5SRC}/lib/kadm5
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
LDADD+=	-lhdb -lkadm5srv -lkrb5 -lkrb -lkdb ${LIBROKEN} -lasn1 -lcrypto \
	-lcom_err -lhdb -lkafs

.include <bsd.prog.mk>
