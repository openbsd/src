#	$OpenBSD: Makefile,v 1.32 2013/07/19 13:41:23 eric Exp $

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

SRCS=	enqueue.c parser.c log.c envelope.c crypto.c
SRCS+=	queue_backend.c queue_fsqueue.c
SRCS+=	smtpctl.c util.c
SRCS+=	compress_backend.c compress_gzip.c
SRCS+=	to.c expand.c tree.c

LDADD+=	-lutil -lz -lcrypto
DPADD+=	${LIBUTIL} ${LIBZ} ${LIBCRYPTO}
.include <bsd.prog.mk>
