#	$OpenBSD: Makefile,v 1.10 2002/05/31 03:46:35 pjanzen Exp $

PROG=	robots
CFLAGS+=-DMAX_PER_UID=5
SRCS=	extern.c init_field.c main.c make_level.c move.c move_robs.c \
	play_level.c query.c rnd_pos.c score.c
MAN=	robots.6
DPADD=	${LIBCURSES}
LDADD=	-lcurses
BINMODE=2555

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/games/robots_roll

.include <bsd.prog.mk>
