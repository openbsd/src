#	$OpenBSD: Makefile,v 1.10 2009/10/28 07:36:49 deraadt Exp $

PROG=	more
CFLAGS+= -D_USE_OLD_CURSES_ -I.
DPADD=	${LIBOCURSES}
LDADD=	-locurses
CLEANFILES+=morehelp.h

beforedepend: morehelp.h

morehelp.h: ${.CURDIR}/more.help
	echo -n 'const char *more_help = "' > morehelp.h
	sed -e 's/[\\"]/\\&/g' -e 's/$$/\\n\\/' ${.CURDIR}/more.help >> morehelp.h
	echo '";' >> morehelp.h

more.o: morehelp.h

.include <bsd.prog.mk>
