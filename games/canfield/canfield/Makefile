#	$OpenBSD: Makefile,v 1.5 2000/07/23 21:49:03 pjanzen Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	canfield
MAN=	canfield.6
DPADD=	${LIBCURSES}
LDADD=	-lcurses
HIDEGAME=hidegame
MLINKS=	canfield.6 cfscores.6

beforeinstall:
	@if [ ! -f ${DESTDIR}/var/games/cfscores ]; then \
	    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 664 \
		/dev/null ${DESTDIR}/var/games/cfscores ; \
	fi

.include "../../Makefile.inc"
.include <bsd.prog.mk>
