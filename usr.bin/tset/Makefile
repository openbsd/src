#	$OpenBSD: Makefile,v 1.5 1998/07/25 19:27:37 millert Exp $

PROG=	tset
SRCS=	map.c misc.c set.c term.c tset.c wrterm.c

DPADD=	${LIBOLDCURSES}
LDADD=	-locurses
LINKS=	${BINDIR}/tset ${BINDIR}/reset
MLINKS=	tset.1 reset.1

.include <bsd.prog.mk>
