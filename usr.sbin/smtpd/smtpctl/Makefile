#	$OpenBSD: Makefile,v 1.8 2009/03/23 15:27:39 deraadt Exp $

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
LDADD+=	-lutil
DPADD+=	${LIBUTIL}
.include <bsd.prog.mk>
