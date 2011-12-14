#	$OpenBSD: Makefile,v 1.21 2011/12/14 18:42:27 eric Exp $

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

SRCS=	enqueue.c parser.c log.c
SRCS+=	queue_backend.c queue_fsqueue.c queue_fsqueue_ascii.c
SRCS+=	smtpctl.c stats.c util.c

LDADD+=	-lutil
DPADD+=	${LIBUTIL} ${LIBEVENT}
.include <bsd.prog.mk>
