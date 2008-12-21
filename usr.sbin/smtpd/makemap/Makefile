#	$OpenBSD: Makefile,v 1.4 2008/12/21 12:59:09 jacekm Exp $

.PATH:		${.CURDIR}/..

PROG=	makemap
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin
MAN=	makemap.8

CFLAGS+=	-g3 -ggdb -I${.CURDIR}/..
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded

LINKS=	${BINDIR}/makemap ${BINDIR}/newaliases
MLINKS=	makemap.8 newaliases.8

SRCS=	makemap.c aliases.c map.c log.c util.c
LDFLAGS=	-lutil
.include <bsd.prog.mk>
