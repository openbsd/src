#	$OpenBSD: Makefile,v 1.5 2002/05/23 18:43:00 deraadt Exp $

PROG=	quiz
MAN=	quiz.6
SRCS=	quiz.c rxp.c
CATS=	africa america areas arith asia babies bard chinese collectives \
	ed elements europe flowers greek inca index latin locomotive \
	midearth morse mult murders poetry posneg pres province seq-easy \
	seq-hard sexes sov spell state trek ucc

beforeinstall:
	(cd ${.CURDIR}/datfiles; ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} \
	    -g ${BINGRP} -m 444 ${CATS} ${DESTDIR}/usr/share/games/quiz.db)

.include <bsd.prog.mk>
