#	$OpenBSD: Makefile,v 1.6 1998/12/31 23:43:04 millert Exp $

PROG=	bs
MAN=	bs.6
DPADD=	${LIBCURSES}
LDADD=	-lcurses
HIDEGAME=hidegame
# XXX - -O2 exposes a gcc optimizer bug
CFLAGS= -O ${PIPE} ${DEBUG}

.include <bsd.prog.mk>
