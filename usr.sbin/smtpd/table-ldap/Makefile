#	$OpenBSD: Makefile,v 1.4 2014/07/08 13:49:09 eric Exp $

.PATH: ${.CURDIR}/..

PROG=	table-ldap

SRCS=	table_ldap.c
SRCS+=	table_api.c
SRCS+=	aldap.c
SRCS+=	ber.c
SRCS+=	dict.c
SRCS+=	log.c

NOMAN=	noman

BINDIR=	/usr/libexec/smtpd

DPADD=	${LIBUTIL}
LDADD=	-lutil

CFLAGS+=	-I${.CURDIR}/..
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
CFLAGS+=	-DNO_IO
#CFLAGS+=	-Werror # during development phase (breaks some archs) 

.include <bsd.prog.mk>
