#	$OpenBSD: Makefile,v 1.30 2013/01/26 09:37:24 gilles Exp $

.PATH:		${.CURDIR}/..

PROG=	smtpctl
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/sbin
MAN=	smtpctl.8

CFLAGS+=	-g3 -ggdb -I${.CURDIR}/..
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded
CFLAGS+=	-DNO_IO

SRCS=	enqueue.c parser.c log.c envelope.c
SRCS+=	queue_backend.c queue_fsqueue.c
SRCS+=	smtpctl.c util.c
SRCS+=	compress_backend.c compress_gzip.c
SRCS+=	to.c expand.c tree.c

LDADD+=	-lutil -lz
DPADD+=	${LIBUTIL} ${LIBZ}
.include <bsd.prog.mk>
