#	$OpenBSD: Makefile,v 1.1 2013/07/20 09:06:46 eric Exp $

.PATH: ${.CURDIR}/..

PROG=	table-ldap

SRCS=	table_ldap.c
SRCS+=	table_api.c
SRCS+=	aldap.c
SRCS+=	ber.c
SRCS+=	log.c

NOMAN=	noman

BINDIR=	/usr/libexec/smtpd

DPADD=	${LIBUTIL}
LDADD=	-lutil

CFLAGS+=	-g3 -ggdb -I${.CURDIR}/..
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded
CFLAGS+=	-DNO_IO
#CFLAGS+=	-Werror # during development phase (breaks some archs) 

.include <bsd.prog.mk>
