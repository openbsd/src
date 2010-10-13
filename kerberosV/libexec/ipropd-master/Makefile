# $OpenBSD: Makefile,v 1.9 2010/10/13 08:14:22 jsg Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	ipropd-master
MAN=	iprop.8
MLINKS+=iprop.8 ipropd-master.8
MLINKS+=iprop.8 ipropd-slave.8

SRCS=	ipropd_master.c
SRCS+=	${PRINTVER}
SRCS+=	rtbl.c

.PATH:	${KRB5SRC}/lib/kadm5
.PATH:	${ROKENSRC}

CFLAGS+=-I${KRB5SRC}/lib/krb5

DPADD+= ${LIBKADM5SRV} ${LIBKRB5} ${LIBCRYPTO} ${LIBUTIL}
LDADD+=	-lkadm5srv -lkrb5 -lcrypto -lutil

.include <bsd.prog.mk>
