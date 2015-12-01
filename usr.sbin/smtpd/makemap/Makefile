#	$OpenBSD: Makefile,v 1.27 2015/12/01 09:54:52 gilles Exp $

.PATH:		${.CURDIR}/..

PROG=	makemap
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/libexec/smtpd
MAN=	aliases.5 forward.5 makemap.8 newaliases.8

CFLAGS+=	-fstack-protector-all
CFLAGS+=	-I${.CURDIR}/..
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
CFLAGS+=	-DNO_IO

SRCS=	aliases.c
SRCS+=	dict.c
SRCS+=	expand.c
SRCS+=	limit.c
SRCS+=	log.c
SRCS+=	makemap.c
SRCS+=	parse.y
SRCS+=	mailaddr.c
SRCS+=	table.c
SRCS+=	to.c
SRCS+=	tree.c
SRCS+=	util.c

SRCS+=	table_static.c
SRCS+=	table_db.c
SRCS+=	table_getpwnam.c
SRCS+=	table_proc.c

DPADD+= ${LIBUTIL} ${LIBCRYPTO} ${LIBZ}
LDADD+= -lutil -lcrypto -lz
.include <bsd.prog.mk>
