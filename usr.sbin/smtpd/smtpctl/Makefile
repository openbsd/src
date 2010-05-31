#	$OpenBSD: Makefile,v 1.13 2010/05/31 23:38:56 jacekm Exp $

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

SRCS=	smtpctl.c parser.c log.c enqueue.c queue_backend.c util.c client.c
LDADD+=	-lutil -levent
DPADD+=	${LIBUTIL} ${LIBEVENT}
.include <bsd.prog.mk>
