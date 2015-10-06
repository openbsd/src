#	$OpenBSD: Makefile,v 1.4 2015/10/06 00:30:32 deraadt Exp $

.PATH: ${.CURDIR}/..

PROG=	table-sqlite

SRCS=	table_sqlite.c
SRCS+=	table_api.c
SRCS+=	dict.c
SRCS+=	log.c

NOMAN=	noman

BINDIR=	/usr/libexec/smtpd

DPADD=	${LIBUTIL} ${LIBSQLITE3}
LDADD=	-lutil -lsqlite3

CFLAGS+=   -fstack-protector-all
CFLAGS+=	-I${.CURDIR}/..
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
CFLAGS+=	-DNO_IO
#CFLAGS+=	-Werror # during development phase (breaks some archs) 

.include <bsd.prog.mk>
