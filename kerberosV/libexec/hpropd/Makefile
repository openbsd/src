# $OpenBSD: Makefile,v 1.8 2010/10/13 08:14:22 jsg Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	hpropd
MAN=	${PROG}.8

SRCS=	hpropd.c
SRCS+=	${PRINTVER}
SRCS+=	mini_inetd.c socket.c tm2time.c 

.PATH:	${KRB5SRC}/kdc
.PATH:	${ROKENSRC}

CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5

DPADD+= ${LIBKADM5SRV} ${LIBKRB5} ${LIBCRYPTO}
LDADD+=	-lkadm5srv -lkrb5 -lcrypto

.include <bsd.prog.mk>
