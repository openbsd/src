#	$OpenBSD: Makefile,v 1.24 2012/08/25 23:35:09 chl Exp $

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
SRCS+=	compress_backend.c compress_zlib.c encrypt.c

LDADD+=	-lutil -lz
DPADD+=	${LIBUTIL} ${LIBEVENT}
.include <bsd.prog.mk>
