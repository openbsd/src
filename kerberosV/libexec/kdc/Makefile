# $OpenBSD: Makefile,v 1.3 2001/06/22 16:12:17 hin Exp $

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
LDADD+=	-lhdb -lkrb5 -lkrb -ldes ${LIBROKEN} -lasn1 -lcrypto

.include <bsd.prog.mk>
