#	$OpenBSD: Makefile,v 1.5 2010/11/03 14:17:01 martinh Exp $

.PATH:		${.CURDIR}/../ldapd

PROG=		ldapctl
MAN=		ldapctl.8
SRCS=		ldapctl.c parse.y btree.c log.c ber.c util.c \
		index.c attributes.c schema.c syntax.c matching.c

LDADD=		-levent -lcrypto -lz -lutil
DPADD=		${LIBEVENT} ${LIBCRYPTO} ${LIBZ} ${LIBUTIL}
CFLAGS+=	-I${.CURDIR}/../ldapd
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare

.include <bsd.prog.mk>
