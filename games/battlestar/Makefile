#	$OpenBSD: Makefile,v 1.8 2002/05/31 03:46:35 pjanzen Exp $

PROG=	battlestar
SRCS=	battlestar.c com1.c com2.c com3.c com4.c com5.c com6.c com7.c \
	init.c cypher.c getcom.c parse.c room.c save.c fly.c misc.c \
	globals.c dayfile.c nightfile.c dayobjs.c nightobjs.c words.c
MAN=	battlestar.6
DPADD=	${LIBCURSES}
LDADD=	-lcurses
BINGRP=	games
BINMODE=2555

beforeinstall: 
	@if [ ! -f ${DESTDIR}/var/games/battlestar.log ]; then \
	    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 664 \
		/dev/null ${DESTDIR}/var/games/battlestar.log ; \
	else \
                true ; \
        fi

.include <bsd.prog.mk>
