#	$OpenBSD: Makefile,v 1.4 1998/07/22 18:01:47 millert Exp $

PROG=	bs
MAN=	bs.6
DPADD=	${LIBCURSES} ${LIBTERMLIB}
LDADD=	-lcurses -ltermlib
HIDEGAME=hidegame
# XXX - -O2 exposes a gcc optimizer bug
CFLAGS= -O ${DEBUG}

.include <bsd.prog.mk>
