#	$NetBSD: Makefile,v 1.9 1995/05/11 22:00:50 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	quiz
MAN=	quiz.6
SRCS=	quiz.c rxp.c
CATS=	africa america areas arith asia babies bard chinese collectives \
	ed elements europe flowers greek inca index latin locomotive \
	midearth morse mult murders poetry posneg pres province seq-easy \
	seq-hard sexes sov spell state trek ucc
HIDEGAME=hidegame

beforeinstall:
	(cd ${.CURDIR}/datfiles; ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} \
	    -g ${BINGRP} -m 444 ${CATS} ${DESTDIR}/usr/share/games/quiz.db)

.include <bsd.prog.mk>
