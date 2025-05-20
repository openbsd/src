#	$OpenBSD: Makefile,v 1.2 2025/05/20 09:35:36 kn Exp $

PROG=	watch

# XXX: why is this needed?
CFLAGS+= -D_XOPEN_SOURCE_EXTENDED -Wall

LDADD+= -lcurses
DPADD+= ${LIBCURSES}

.include <bsd.prog.mk>
