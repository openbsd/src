# $OpenBSD: Makefile,v 1.8 2009/08/17 09:40:37 jsg Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kstash
MAN=	${PROG}.8

SRCS=	kstash.c \
	sl.c
SRCS+=	${PRINTVER}

.PATH:	${KRB5SRC}/kdc
.PATH:	${KRB5SRC}/lib/sl

CFLAGS+=-I${KRB5SRC}/lib/krb5

DPADD+= ${LIBEDIT} ${LIBCURSES} ${LIBDES} ${LIBKRB5} \
	${LIBCRYPTO}
LDADD+=	-ledit -lcurses -ldes -lkrb5 \
	-lcrypto

.include <bsd.prog.mk>
