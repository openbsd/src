#	$OpenBSD: Makefile,v 1.1 2025/05/20 04:33:06 job Exp $

PROG=	watch

# XXX: why is this needed?
CFLAGS+= -D_XOPEN_SOURCE_EXTENDED

LDADD+= -lcurses
DPADD+= ${LIBCURSES}

.include <bsd.prog.mk>
