# $OpenBSD: Makefile,v 1.10 2003/05/14 07:20:34 mho Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kdc
MAN=	${PROG}.8

SRCS=	524.c \
	config.c \
	connect.c \
	kdc_locl.h \
	kerberos5.c \
	kerberos4.c \
	log.c \
	main.c \
	misc.c \
	rx.h

.PATH:	${KRB5SRC}/kdc
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/kdc
DPADD+=	${LIBHDB} ${LIBKRB5} ${LIBDES} ${LIBASN1} ${LIBCRYPTO} \
	${LIBCOM_ERR} ${LIBUTIL}
LDADD+=	-lhdb -lkrb5 -ldes ${LIBROKEN} -lasn1 -lcrypto \
	-lcom_err -lutil

.include <bsd.prog.mk>
