#	$OpenBSD: Makefile,v 1.5 2009/01/27 22:54:01 gilles Exp $

.PATH:		${.CURDIR}/..

PROG=	smtpctl
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin
MAN=	smtpctl.8

CFLAGS+=	-g3 -ggdb -I${.CURDIR}/..
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded

SRCS=	smtpctl.c parser.c buffer.c imsg.c log.c showqueue.c enqueue.c queue_shared.c util.c
LDFLAGS=	-lutil
.include <bsd.prog.mk>
