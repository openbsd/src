# $OpenBSD: Makefile,v 1.2 2001/06/27 22:02:26 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	hpropd
MAN=	${PROG}.8

SRCS=	hpropd.c

.PATH:	${KRB5SRC}/kdc
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
LDADD+=	-lhdb -lkadm5srv -lkrb5 -lkdb -lkrb -lkdb ${LIBROKEN} -lasn1 -lcrypto \
	-lcom_err

.include <bsd.prog.mk>
