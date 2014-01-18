#	$OpenBSD: Makefile,v 1.2 2014/01/18 05:54:51 martynas Exp $

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

CFLAGS+=	-g3 -ggdb -I${.CURDIR}/..
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
CFLAGS+=	-DNO_IO
#CFLAGS+=	-Werror # during development phase (breaks some archs) 

.include <bsd.prog.mk>
