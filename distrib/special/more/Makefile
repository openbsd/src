#	$OpenBSD: Makefile,v 1.6 2003/05/28 20:05:41 mickey Exp $

PROG=	more
MAN=
CFLAGS+=-D_USE_OLD_CURSES_ -I.
DPADD=	${LIBOCURSES}
LDADD=	-locurses
CLEANFILES+=morehelp.h

beforedepend: morehelp.h

morehelp.h: ${.CURDIR}/more.help
	echo -n 'const char *more_help = "' > morehelp.h
	cat  ${.CURDIR}/more.help >> morehelp.h
	echo '";' >> morehelp.h

.include <bsd.prog.mk>
