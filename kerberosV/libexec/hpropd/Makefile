# $OpenBSD: Makefile,v 1.7 2003/07/14 16:14:43 mho Exp $

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

DPADD+= ${LIBKADM5SRV} ${LIBKRB5} ${LIBCRYPTO} ${LIBDES}
LDADD+=	-lkadm5srv -lkrb5 -lcrypto -ldes

.include <bsd.prog.mk>
