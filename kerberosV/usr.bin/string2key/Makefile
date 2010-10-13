# $OpenBSD: Makefile,v 1.5 2010/10/13 08:14:22 jsg Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	string2key
MAN=	${PROG}.8

SRCS=	string2key.c
SRCS+=	${PRINTVER}
SRCS+=	estrdup.c

.PATH:	${KRB5SRC}/kdc
.PATH:	${ROKENSRC}

CFLAGS+=-I${KRB5SRC}/kdc -I${KRB5SRC}/lib/krb5

DPADD+=	${LIBKRB5} ${LIBCRYPTO}
LDADD+=	-lkrb5 -lcrypto

.include <bsd.prog.mk>
