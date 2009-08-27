#	$OpenBSD: Makefile,v 1.9 2009/08/27 11:37:30 jacekm Exp $

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

SRCS=	smtpctl.c parser.c buffer.c imsg.c log.c enqueue.c		\
	queue_shared.c util.c client.c
LDADD+=	-lutil
DPADD+=	${LIBUTIL}
.include <bsd.prog.mk>
