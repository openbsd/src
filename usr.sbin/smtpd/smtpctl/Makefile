#	$OpenBSD: Makefile,v 1.26 2012/08/26 18:56:08 gilles Exp $

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

SRCS=	enqueue.c parser.c log.c envelope.c
SRCS+=	queue_backend.c queue_fsqueue.c
SRCS+=	smtpctl.c util.c
SRCS+=	compress_backend.c compress_gzip.c

LDADD+=	-lutil -lz
DPADD+=	${LIBUTIL} ${LIBZ}
.include <bsd.prog.mk>
