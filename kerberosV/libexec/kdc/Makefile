# $OpenBSD: Makefile,v 1.6 2001/09/29 17:45:40 jakob Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kdc
MAN=	${PROG}.8

SRCS=	524.c \
	config.c \
	connect.c \
	kdc_locl.h \
	kerberos4.c \
	kerberos5.c \
	log.c \
	main.c \
	misc.c \
	rx.h

.PATH:	${KRB5SRC}/kdc
CFLAGS+=-I${KRB5SRC}/lib/krb5
LDADD+=	-lhdb -lkrb5 -lkrb -ldes ${LIBROKEN} -lasn1 -lcrypto -lcom_err -lkafs -lutil

.include <bsd.prog.mk>
