#	$OpenBSD: Makefile,v 1.6 1998/07/24 00:11:05 millert Exp $

PROG=	tput
DPADD=	${LIBCURSES}
LDADD=	-lcurses
MLINKS=	tput.1 clear.1
LINKS= ${BINDIR}/tput ${BINDIR}/clear

.include <bsd.prog.mk>
