# $OpenBSD: Makefile,v 1.7 2003/07/14 16:14:44 mho Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	ipropd-master
MAN=	${PROG}.8
NOMAN=

SRCS=	ipropd_master.c
SRCS+=	${PRINTVER}
SRCS+=	rtbl.c

.PATH:	${KRB5SRC}/lib/kadm5
.PATH:	${ROKENSRC}

CFLAGS+=-I${KRB5SRC}/lib/krb5

DPADD+= ${LIBKADM5SRV} ${LIBKRB5} ${LIBCRYPTO} ${LIBUTIL} ${LIBDES}
LDADD+=	-lkadm5srv -lkrb5 -lcrypto -lutil -ldes

.include <bsd.prog.mk>
