#	$NetBSD: Makefile,v 1.7 1995/04/22 10:58:17 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	trek
SRCS=	abandon.c attack.c autover.c capture.c check_out.c checkcond.c \
	compkl.c computer.c damage.c damaged.c dcrept.c destruct.c \
	dock.c dumpgame.c dumpme.c dumpssradio.c events.c externs.c \
	getcodi.c getpar.c help.c impulse.c initquad.c kill.c klmove.c \
	lose.c lrscan.c main.c move.c nova.c out.c phaser.c play.c ram.c \
	ranf.c rest.c schedule.c score.c setup.c setwarp.c \
	shield.c snova.c srscan.c systemname.c torped.c utility.c \
	visual.c warp.c win.c cgetc.c
MAN=	trek.6
DPADD=	${LIBM}
LDADD=	-lm
HIDEGAME=hidegame

.if make(install)
SUBDIR+= USD.doc
.endif

.include <bsd.prog.mk>
