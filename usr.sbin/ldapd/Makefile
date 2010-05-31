#	$OpenBSD: Makefile,v 1.1 2010/05/31 17:36:31 martinh Exp $

PROG=		ldapd
MAN=		ldapd.8 ldapd.conf.5
SRCS=		ber.c log.c control.c \
		util.c ldapd.c ldape.c conn.c attributes.c namespace.c \
		btree.c filter.c search.c parse.y \
		auth.c modify.c index.c ssl.c ssl_privsep.c compact.c \
		validate.c uuid.c

LDADD=		-levent -lcrypto -lssl -lz -lutil
DPADD=		${LIBEVENT} ${LIBCRYPTO} ${LIBSSL} ${LIBZ} ${LIBUTIL}
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
CLEANFILES+=	y.tab.h parse.c

.include <bsd.prog.mk>

