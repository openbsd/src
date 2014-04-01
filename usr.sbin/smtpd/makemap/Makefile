#	$OpenBSD: Makefile,v 1.24 2014/04/01 23:14:25 fgsch Exp $

.PATH:		${.CURDIR}/..

PROG=	makemap
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/libexec/smtpd
MAN=	aliases.5 forward.5 makemap.8 newaliases.8

CFLAGS+=	-I${.CURDIR}/..
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
CFLAGS+=	-DNO_IO

SRCS=	aliases.c dict.c expand.c limit.c log.c makemap.c parse.y \
	table.c to.c tree.c util.c

SRCS+=	table_static.c
SRCS+=	table_db.c
SRCS+=	table_getpwnam.c
SRCS+=	table_proc.c

DPADD+= ${LIBUTIL} ${LIBCRYPTO} ${LIBZ}
LDADD+= -lutil -lcrypto -lz
.include <bsd.prog.mk>
