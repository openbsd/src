#	$OpenBSD: Makefile,v 1.7 1997/04/27 20:56:51 millert Exp $
#	@(#)Makefile	5.11 (Berkeley) 6/25/90

PROG=	more
DPADD=	${LIBTERM} ${LIBCOMPAT}
LDADD=	-ltermcap -lcompat
MLINKS=	more.1 page.1
LINKS=	${BINDIR}/more ${BINDIR}/page

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/more.help ${DESTDIR}/usr/share/misc/more.help

.include <bsd.prog.mk>
