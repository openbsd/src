#	$OpenBSD: Makefile,v 1.5 1996/10/14 03:51:05 etheisen Exp $
#	@(#)Makefile	5.11 (Berkeley) 6/25/90

PROG=	more
DPADD=	${LIBTERM} ${LIBCOMPAT}
LDADD=	-ltermcap -lcompat
MLINKS=	more.1 page.1
LINKS=	${BINDIR}/more ${BINDIR}/page

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/more.help \
	    ${DESTDIR}/usr/share/misc/more.help

.include <bsd.prog.mk>
