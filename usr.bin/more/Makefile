#	$OpenBSD: Makefile,v 1.8 1997/09/21 11:50:08 deraadt Exp $

PROG=	more
DPADD=	${LIBTERM} ${LIBCOMPAT}
LDADD=	-ltermcap -lcompat
MLINKS=	more.1 page.1
LINKS=	${BINDIR}/more ${BINDIR}/page

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/more.help ${DESTDIR}/usr/share/misc/more.help

.include <bsd.prog.mk>
