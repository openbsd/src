# $OpenBSD: Makefile,v 1.1 2001/06/25 02:29:53 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	hprop
MAN=	${PROG}.8

SRCS=	hprop.c mit_dump.c v4_dump.c hprop.h kadb.h

.PATH:	${KRB5SRC}/kdc
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
LDADD+=	-lhdb -lkadm5srv -lkrb5 -lkrb -lkdb ${LIBROKEN} -lasn1 -lcrypto \
	-lcom_err -lhdb

.include <bsd.prog.mk>
