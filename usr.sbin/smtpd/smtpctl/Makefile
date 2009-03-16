#	$OpenBSD: Makefile,v 1.7 2009/03/16 22:03:42 jacekm Exp $

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

SRCS=	smtpctl.c parser.c buffer.c imsg.c log.c enqueue.c queue_shared.c util.c
LDFLAGS=	-lutil
.include <bsd.prog.mk>
