#	$NetBSD: Makefile,v 1.5 1995/03/21 15:06:38 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	battlestar
SRCS=	battlestar.c com1.c com2.c com3.c com4.c com5.c com6.c com7.c \
	init.c cypher.c getcom.c parse.c room.c save.c fly.c misc.c \
	globals.c dayfile.c nightfile.c dayobjs.c nightobjs.c words.c
MAN=	battlestar.6
DPADD=	${LIBCURSES} ${LIBTERMLIB}
LDADD=	-lcurses -ltermlib
HIDEGAME=hidegame
BINGRP=	games

beforeinstall: 
	@if [ ! -f ${DESTDIR}/var/games/battlestar.log ]; then \
	    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 664 \
		/dev/null ${DESTDIR}/var/games/battlestar.log ; \
	else \
                true ; \
        fi

.include <bsd.prog.mk>
