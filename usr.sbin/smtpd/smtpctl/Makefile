#	$OpenBSD: Makefile,v 1.25 2012/08/26 13:38:43 gilles Exp $

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
SRCS+=	compress_backend.c compress_gzip.c encrypt.c

LDADD+=	-lutil -lz
DPADD+=	${LIBUTIL} ${LIBEVENT}
.include <bsd.prog.mk>
