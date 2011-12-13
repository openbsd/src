#	$OpenBSD: Makefile,v 1.12 2011/12/13 23:00:52 eric Exp $

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

SRCS=	parse.y makemap.c aliases.c expand.c log.c util.c map.c \
	map_db.c map_stdio.c

DPADD+= ${LIBUTIL}
LDADD+= -lutil
.include <bsd.prog.mk>
