# $OpenBSD: Makefile,v 1.8 2003/07/14 16:14:44 mho Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	ipropd-slave
MAN=	${PROG}.8
NOMAN=

SRCS=	ipropd_slave.c
SRCS+=	${PRINTVER}

.PATH:	${KRB5SRC}/lib/kadm5

CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5

DPADD+= ${LIBKADM5SRV} ${LIBKRB5} ${LIBCRYPTO} ${LIBUTIL} ${LIBDES}
LDADD+=	-lkadm5srv -lkrb5 -lcrypto -lutil -ldes

.include <bsd.prog.mk>
