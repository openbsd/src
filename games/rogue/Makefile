#	$OpenBSD: Makefile,v 1.9 2002/07/18 07:13:56 pjanzen Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	rogue
CFLAGS+=-fwritable-strings
SRCS=	hit.c init.c inventory.c level.c machdep.c main.c \
	message.c monster.c move.c object.c pack.c play.c random.c ring.c \
	room.c save.c score.c spec_hit.c throw.c trap.c use.c zap.c
DPADD=	${LIBCURSES}
LDADD=	-lcurses
MAN=	rogue.6
BINMODE=2555

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
