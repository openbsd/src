#	$OpenBSD: Makefile,v 1.5 2003/05/28 19:19:41 millert Exp $

PROG=	more
MAN=
CFLAGS+=-D_USE_OLD_CURSES_ -I.
DPADD=	${LIBOCURSES}
LDADD=	-locurses
CLEANFILES+=morehelp.h

beforedepend: morehelp.h

morehelp.h: ${.CURDIR}/more.help
	sed 's/\(^.*$$\)/\"\1\",/' ${.CURDIR}/more.help > morehelp.h

.include <bsd.prog.mk>
