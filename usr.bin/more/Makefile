#	$OpenBSD: Makefile,v 1.9 1998/07/24 00:10:55 millert Exp $

PROG=	more
DPADD=	${LIBCURSES} ${LIBCOMPAT}
LDADD=	-lcurses -lcompat
MLINKS=	more.1 page.1
LINKS=	${BINDIR}/more ${BINDIR}/page

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/more.help ${DESTDIR}/usr/share/misc/more.help

.include <bsd.prog.mk>
