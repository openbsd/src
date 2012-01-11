#	$OpenBSD: Makefile,v 1.22 2012/01/11 22:24:37 gilles Exp $

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
SRCS+=	smtpctl.c stats.c util.c

LDADD+=	-lutil
DPADD+=	${LIBUTIL} ${LIBEVENT}
.include <bsd.prog.mk>
