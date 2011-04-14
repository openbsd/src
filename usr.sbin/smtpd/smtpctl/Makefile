#	$OpenBSD: Makefile,v 1.17 2011/04/14 20:11:08 gilles Exp $

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
CFLAGS+=	-DCLIENT_NO_SSL

SRCS=	smtpctl.c parser.c log.c enqueue.c queue_shared.c util.c client.c queue_backend.c queue_fsqueue.c
LDADD+=	-lutil -levent
DPADD+=	${LIBUTIL} ${LIBEVENT}
.include <bsd.prog.mk>
