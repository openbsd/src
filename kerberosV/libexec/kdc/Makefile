# $OpenBSD: Makefile,v 1.2 2001/05/25 08:53:40 hin Exp $

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
LDADD+=	${LIBROKEN} -lhdb -lkrb5 -lkrb -ldes

.include <bsd.prog.mk>
