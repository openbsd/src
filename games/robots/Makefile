#	$OpenBSD: Makefile,v 1.5 1997/04/27 20:55:47 millert Exp $
#	$NetBSD: Makefile,v 1.5 1995/04/22 10:08:46 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	robots
CFLAGS+=-DMAX_PER_UID=5
SRCS=	extern.c init_field.c main.c make_level.c move.c move_robs.c \
	play_level.c query.c rnd_pos.c score.c flush_in.c
MAN=	robots.6
DPADD=	${LIBCURSES} ${LIBTERMLIB}
LDADD=	-lcurses -ltermlib
HIDEGAME=hidegame

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/games/robots_roll

.include <bsd.prog.mk>
