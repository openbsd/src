# $OpenBSD: Makefile,v 1.4 2002/03/04 23:26:43 deraadt Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	hprop
MAN=	${PROG}.8

SRCS=	hprop.c mit_dump.c v4_dump.c hprop.h kadb.h

.PATH:	${KRB5SRC}/kdc
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
LDADD+=	-lhdb -lkadm5srv -lkrb5 -lkdb -lkrb -lkdb ${LIBROKEN} -lasn1 \
	-lcom_err -lkafs -ldes -lcrypto

.include <bsd.prog.mk>
