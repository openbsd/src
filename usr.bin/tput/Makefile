#	$OpenBSD: Makefile,v 1.11 1999/06/29 19:39:39 millert Exp $

PROG=	tput
DPADD=	${LIBCURSES}
LDADD=	-lcurses
MLINKS=	tput.1 clear.1
LINKS= ${BINDIR}/tput ${BINDIR}/clear

.include <bsd.prog.mk>
