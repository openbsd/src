#	$OpenBSD: Makefile,v 1.9 1999/03/06 20:19:21 millert Exp $

PROG=	tput
DPADD=	${LIBCURSES}
LDADD=	-lcurses
MLINKS=	tput.1 clear.1
LINKS= ${BINDIR}/tput ${BINDIR}/clear

.include <bsd.prog.mk>
