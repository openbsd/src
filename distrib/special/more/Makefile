#	$OpenBSD: Makefile,v 1.2 2003/05/06 00:23:03 millert Exp $

PROG=	more
MAN=
CFLAGS+=-D_USE_OLD_CURSES_
DPADD=	${LIBOCURSES}
LDADD=	-locurses

.include <bsd.prog.mk>
