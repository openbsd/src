#	$OpenBSD: Makefile,v 1.6 2016/05/01 10:40:27 tb Exp $

.PATH:		${.CURDIR}/../ldapd

PROG=		ldapctl
MAN=		ldapctl.8
SRCS=		ldapctl.c parse.y btree.c log.c ber.c util.c \
		index.c attributes.c schema.c syntax.c matching.c

LDADD=		-ltls -levent -lz -lutil
DPADD=		${LIBTLS} ${LIBEVENT} ${LIBZ} ${LIBUTIL}
CFLAGS+=	-I${.CURDIR}/../ldapd
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare

.include <bsd.prog.mk>
