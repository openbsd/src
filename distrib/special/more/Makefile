#	$OpenBSD: Makefile,v 1.1 2003/04/17 22:53:12 millert Exp $

PROG=	more
MAN=
CFLAGS+=-D_USE_OLD_CURSES_
DPADD=	${LIBOCURSES} ${LIBCOMPAT}
LDADD=	-locurses -lcompat

.include <bsd.prog.mk>
