#	$OpenBSD: Makefile,v 1.10 2011/05/21 18:43:08 gilles Exp $

.PATH:		${.CURDIR}/..

PROG=	makemap
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/libexec/smtpd
MAN=	makemap.8 newaliases.8

CFLAGS+=	-g3 -ggdb -I${.CURDIR}/..
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded

SRCS=	parse.y makemap.c aliases.c expand.c map.c map_backend.c map_backend_db.c map_backend_stdio.c log.c util.c
DPADD+= ${LIBUTIL}
LDADD+= -lutil
.include <bsd.prog.mk>
