#	$OpenBSD: Makefile,v 1.2 1996/03/09 01:33:30 niklas Exp $
#	$NetBSD: Makefile,v 1.4 1995/03/24 03:58:22 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	phantasia
SRCS=	main.c fight.c io.c interplayer.c gamesupport.c misc.c phantglobs.c
DPADD=	${LIBM} ${LIBCURSES} ${LIBTERM} ${LIBCOMPAT}
LDADD=	-lm -lcurses -ltermlib -lcompat
HIDEGAME=hidegame
MAN=	phantasia.6
CLEANFILES+=map setup setup.o

all: setup phantasia

setup.o: setup.c
	${HOSTCC} -c setup.c -o ${.TARGET}

phantglobs.o.bld: phantglobs.c
	${HOSTCC} -c phantglobs.c -o ${.TARGET}

setup: phantglobs.o.bld setup.o monsters.asc ${LIBM} 
	${HOSTCC} phantglobs.o.bld setup.o -o ${.TARGET} -lm

beforeinstall: 
	./setup -m ${.CURDIR}/monsters.asc
	chown games.bin ${DESTDIR}/var/games/phantasia/*

# Make Phantasia map.  Change the map commands reflect your installation.
# PLOTDEVICE is used for plotting the map.  Change as appropriate.

map: map.c
	${HOSTCC} -O ${.CURDIR}/map.c -lplot -o ${.TARGET}
	./map | plot > /dev/tty

# XXX this rule shouldn't be here.
phantasia.cat6: phantasia.6
	tbl ${.CURDIR}/phantasia.6 | nroff -man > ${.TARGET}

.include <bsd.prog.mk>
