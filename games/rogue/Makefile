#	$OpenBSD: Makefile,v 1.5 1998/08/22 08:55:24 pjanzen Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	rogue
CFLAGS+=-DUNIX -fwritable-strings
SRCS=	curses.c hit.c init.c inventory.c level.c machdep.c main.c \
	message.c monster.c move.c object.c pack.c play.c random.c ring.c \
	room.c save.c score.c spec_hit.c throw.c trap.c use.c zap.c
DPADD=	${LIBCURSES}
LDADD=	-lcurses
HIDEGAME=hidegame
MAN=	rogue.6

.if make(install)
SUBDIR+= USD.doc
.endif

beforeinstall: 
	@if [ ! -f ${DESTDIR}/var/games/rogue.scores ]; then \
	    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 664 \
		/dev/null ${DESTDIR}/var/games/rogue.scores ; \
	else \
                true ; \
        fi

.include <bsd.prog.mk>
