#	$OpenBSD: Makefile,v 1.4 1998/07/24 00:11:06 millert Exp $

PROG=	tset
SRCS=	map.c misc.c set.c term.c tset.c wrterm.c

DPADD=	${LIBCURSES}
LDADD=	-lcurses
LINKS=	${BINDIR}/tset ${BINDIR}/reset
MLINKS=	tset.1 reset.1

.include <bsd.prog.mk>
