# $OpenBSD: Makefile,v 1.8 2006/04/23 20:46:55 biorn Exp $

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

DPADD+= ${LIBKADM5SRV} ${LIBKRB5} ${LIBCRYPTO} ${LIBUTIL} ${LIBDES}
LDADD+=	-lkadm5srv -lkrb5 -lcrypto -lutil -ldes

.include <bsd.prog.mk>
