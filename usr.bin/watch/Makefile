#	$OpenBSD: Makefile,v 1.3 2025/05/21 08:32:10 florian Exp $

PROG=	watch

# XXX: why is this needed?
CFLAGS+= -D_XOPEN_SOURCE_EXTENDED -Wall

LDADD+= -lcurses -levent
DPADD+= ${LIBCURSES} ${LIBEVENT}

.include <bsd.prog.mk>
