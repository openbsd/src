# $OpenBSD: Makefile,v 1.12 2006/04/14 08:28:55 biorn Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kdc
MAN=	${PROG}.8

SRCS=	524.c \
	config.c \
	connect.c \
	kaserver.c \
	kdc_locl.h \
	kerberos5.c \
	kerberos4.c \
	log.c \
	main.c \
	misc.c \
	rx.h
SRCS+=	${PRINTVER}
SRCS+=	parse_bytes.c

.PATH:	${KRB5SRC}/kdc
.PATH:	${ROKENSRC}

CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/kdc

DPADD+=	${LIBKRB5} ${LIBDES} ${LIBCRYPTO} ${LIBUTIL}
LDADD+=	-lkrb5 -ldes -lcrypto -lutil

.include <bsd.prog.mk>
