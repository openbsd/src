#	$OpenBSD: Makefile,v 1.9 2017/07/03 22:21:47 espie Exp $

.PATH:		${.CURDIR}/../ldapd

PROG=		ldapctl
MAN=		ldapctl.8
SRCS=		ldapctl.c parse.y btree.c log.c logmsg.c ber.c util.c \
		index.c attributes.c schema.c syntax.c matching.c

LDADD=		-levent -ltls -lssl -lcrypto -lz -lutil
DPADD=		${LIBEVENT} ${LIBTLS} ${LIBSSL} ${LIBCRYPTO} ${LIBZ} ${LIBUTIL}
CFLAGS+=	-I${.CURDIR}/../ldapd
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
YFLAGS=

.include <bsd.prog.mk>
