#	$OpenBSD: Makefile,v 1.16 2004/11/11 16:39:00 jolan Exp $

PROG=	phantasia
SRCS=	main.c fight.c io.c interplayer.c gamesupport.c misc.c phantglobs.c
CFLAGS+=-DTERMIOS
DPADD=	${LIBM} ${LIBCURSES}
LDADD=	-lm -lcurses
BINMODE=2555
MAN=	phantasia.6
CLEANFILES+=map setup setup.o phantglobs.o.bld

all: setup phantasia

setup.o: setup.c
	${HOSTCC} -c ${CFLAGS} -o ${.TARGET} ${.CURDIR}/setup.c

phantglobs.o.bld: phantglobs.c
	${HOSTCC} -c ${CFLAGS} -o ${.TARGET} ${.CURDIR}/phantglobs.c

setup: phantglobs.o.bld setup.o monsters.asc ${DPADD} 
	${HOSTCC} ${CFLAGS} ${LDFLAGS} ${LDSTATIC} -o ${.TARGET} \
		  phantglobs.o.bld setup.o ${LDADD}

beforeinstall: 
	./setup -m ${.CURDIR}/monsters.asc
	chown root:games ${DESTDIR}/var/games/phantasia/*

# Make Phantasia map.  Change the map commands reflect your installation.
# PLOTDEVICE is used for plotting the map.  Change as appropriate.

map: map.c
	${HOSTCC} -O ${.CURDIR}/map.c -lplot -o ${.TARGET}
	./map | plot > /dev/tty

.include <bsd.prog.mk>
