#	$OpenBSD: Makefile,v 1.5 1998/07/24 00:10:22 millert Exp $

PROG=	bs
MAN=	bs.6
DPADD=	${LIBCURSES}
LDADD=	-lcurses
HIDEGAME=hidegame
# XXX - -O2 exposes a gcc optimizer bug
CFLAGS= -O ${DEBUG}

.include <bsd.prog.mk>
