#	$OpenBSD: Makefile,v 1.10 1999/03/06 20:27:42 millert Exp $

PROG=	tput
CFLAGS+= -D_USE_OLD_CURSES_
DPADD=	${LIBOLDCURSES}
LDADD=	-locurses
MLINKS=	tput.1 clear.1
LINKS= ${BINDIR}/tput ${BINDIR}/clear

.include <bsd.prog.mk>
