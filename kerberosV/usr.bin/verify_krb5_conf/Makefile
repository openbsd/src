# $OpenBSD: Makefile,v 1.5 2010/10/13 08:14:22 jsg Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	verify_krb5_conf
MAN=	${PROG}.8

SRCS=	verify_krb5_conf.c
SRCS+=	${PRINTVER}
SRCS+=	parse_bytes.c

.PATH:	${KRB5SRC}/lib/krb5
.PATH:	${ROKENSRC}

DPADD+= ${LIBKRB5} ${LIBCRYPTO}
LDADD+=	-lkrb5 -lcrypto

.include <bsd.prog.mk>
