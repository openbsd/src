# $OpenBSD: Makefile,v 1.8 2002/05/11 00:20:19 espie Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kdc
MAN=	${PROG}.8

SRCS=	524.c \
	config.c \
	connect.c \
	kaserver.c \
	kdc_locl.h \
	kerberos4.c \
	kerberos5.c \
	log.c \
	main.c \
	misc.c \
	rx.h

.PATH:	${KRB5SRC}/kdc
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/kdc
DPADD+=	${LIBHDB} ${LIBKRB5} ${LIBKRB} ${LIBDES} ${LIBASN1} ${LIBCRYPTO} \
	${LIBCOM_ERR} ${LIBKAFS} ${LIBUTIL}
LDADD+=	-lhdb -lkrb5 -lkrb -ldes ${LIBROKEN} -lasn1 -lcrypto -lcom_err -lkafs -lutil

.include <bsd.prog.mk>
