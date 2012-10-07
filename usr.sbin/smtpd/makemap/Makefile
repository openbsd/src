#	$OpenBSD: Makefile,v 1.15 2012/10/07 15:46:38 chl Exp $

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
CFLAGS+=	-DNO_IO

SRCS=	parse.y makemap.c aliases.c expand.c log.c util.c map.c \
	map_static.c map_db.c map_stdio.c

DPADD+= ${LIBUTIL} ${LIBCRYPTO}
LDADD+= -lutil -lcrypto
.include <bsd.prog.mk>
