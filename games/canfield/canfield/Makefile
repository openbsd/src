#	$OpenBSD: Makefile,v 1.7 2002/05/23 18:43:00 deraadt Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	canfield
MAN=	canfield.6
DPADD=	${LIBCURSES}
LDADD=	-lcurses
MLINKS=	canfield.6 cfscores.6

beforeinstall:
	@if [ ! -f ${DESTDIR}/var/games/cfscores ]; then \
	    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 664 \
		/dev/null ${DESTDIR}/var/games/cfscores ; \
	else \
	    true ; \
	fi

.include "../../Makefile.inc"
.include <bsd.prog.mk>
