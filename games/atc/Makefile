#	$OpenBSD: Makefile,v 1.9 1998/09/21 07:36:04 pjanzen Exp $

PROG=	atc
CFLAGS+=-DBSD -I${.CURDIR} -I.
SRCS=	extern.c grammar.c graphics.c input.c lex.c list.c log.c \
	main.c tunable.c update.c
MAN=	atc.6
LDADD=	-ll -lm -lcurses
DPADD=	${LIBL} ${LIBM} ${LIBCURSES}
GAMES=	Game_List Killer crossover default easy game_2 \
	Atlantis OHare Tic-Tac-Toe airports box crosshatch game_3 \
	game_4 novice two-corners
CLEANFILES=grammar.c y.tab.h lex.c
HIDEGAME=hidegame

beforeinstall: 
	@if [ ! -d ${DESTDIR}/usr/share/games/atc ]; then \
                /bin/rm -f ${DESTDIR}/usr/share/games/atc ; \
                mkdir -p ${DESTDIR}/usr/share/games/atc ; \
                chown root.wheel ${DESTDIR}/usr/share/games/atc ; \
                chmod 755 ${DESTDIR}/usr/share/games/atc ; \
        else \
                true ; \
        fi
	(cd ${.CURDIR}/games; ${INSTALL} ${INSTALL_COPY} \
	    -o ${BINOWN} -g ${BINGRP} \
	    -m 444 ${GAMES} ${DESTDIR}/usr/share/games/atc)
	@if [ ! -f ${DESTDIR}/var/games/atc_score ]; then \
	    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 664 \
		/dev/null ${DESTDIR}/var/games/atc_score ; \
	else \
                true ; \
        fi

.include <bsd.prog.mk>
