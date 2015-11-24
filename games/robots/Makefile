#	$OpenBSD: Makefile,v 1.11 2015/11/24 03:10:10 deraadt Exp $

PROG=	robots
CFLAGS+=-DMAX_PER_UID=5
SRCS=	extern.c init_field.c main.c make_level.c move.c move_robs.c \
	play_level.c query.c rnd_pos.c score.c
MAN=	robots.6
DPADD=	${LIBCURSES}
LDADD=	-lcurses

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/games/robots_roll

.include <bsd.prog.mk>
