#	$OpenBSD: Makefile,v 1.4 1998/07/24 00:10:40 millert Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	rogue
CFLAGS+=-DUNIX -fwritable-strings
SRCS=	curses.c hit.c init.c inventory.c level.c machdep.c main.c \
	message.c monster.c move.c object.c pack.c play.c random.c ring.c \
	room.c save.c score.c spec_hit.c throw.c trap.c use.c zap.c
DPADD=	${LIBCURSES} ${LIBCOMPAT}
LDADD=	-lcurses -lcompat
HIDEGAME=hidegame
MAN=	rogue.6

.if make(install)
SUBDIR+= USD.doc
.endif

.include <bsd.prog.mk>
