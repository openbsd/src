#	$OpenBSD: Makefile,v 1.14 2002/05/31 03:46:35 pjanzen Exp $

# EXTRA
#	Incorporates code to gather additional performance statistics
# TERMIO
#	Use sysv termio
# TERMIOS
#	Use posix termios
# DOCHECKPOINTS
#	If not defined, checkpoint files are periodically written by the
#	larn process (no forking) if enabled in the .larnopts description
#	file.  Checkpointing is handy on an unreliable system, but takes
#	CPU. Inclusion of DOCHECKPOINTS will cause fork()ing to perform the
#	checkpoints (again if enabled in the .larnopts file).  This usually
#	avoids pauses in larn while the checkpointing is being done (on
#	large machines).
# VER
#	This is the version of the software, example:  12
# SUBVER
#	This is the revision of the software, example:  1
# FLUSHNO=#
#	Set the input queue excess flushing threshold (default 5)
# NOVARARGS
#	Define for systems that don't have varargs (a default varargs will
#	be used).
# MACRORND
#	Define to use macro version of rnd() and rund() (fast and big)
# UIDSCORE
#	Define to use user id's to manage scoreboard.  Leaving this out will
#	cause player id's from the file ".playerids" to be used instead.
#	(.playerids is created upon demand).  Only one entry per id # is
#	allowed in each scoreboard (winning & non-winning).
# NONAP
#	This causes napms() to return immediately instead of delaying n
#	milliseconds.  This define may be needed on some systems if the nap
#	stuff does not work correctly (possible hang).  nap() is primarilly
#	used to delay for effect when casting missile type spells.
# NOLOG
#	Turn off logging.
# NOSPAM
#	Don't send e-mail to players once they've won.

PROG=	larn
MAN=	larn.6
CFLAGS+=-DVER=12 -DSUBVER=0 -DNONAP -DUIDSCORE -DTERMIOS
SRCS=	main.c object.c create.c tok.c display.c global.c data.c io.c \
	monster.c store.c diag.c help.c config.c nap.c bill.c scores.c \
	signal.c moreobj.c movem.c regen.c fortune.c savelev.c
DPADD=	${LIBCURSES}
LDADD=	-lcurses
BINMODE=2555

beforeinstall:
	(cd ${.CURDIR}/datfiles; ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} \
	    -m 444 larnmaze larnopts larn.help \
	    ${DESTDIR}/usr/share/games/larn)
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 660 /dev/null \
	    ${DESTDIR}/var/games/larn/lscore12.0
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 660 /dev/null \
	    ${DESTDIR}/var/games/larn/llog12.0

.include <bsd.prog.mk>
