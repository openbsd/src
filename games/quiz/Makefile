#	$OpenBSD: Makefile,v 1.6 2014/12/02 12:50:04 schwarze Exp $

PROG=	quiz
MAN=	quiz.6
SRCS=	quiz.c rxp.c
CATS=	africa america areas arith asia babies bard chinese collectives \
	ed elements europe flowers greek inca index latin locomotive \
	midearth morse mult murders poetry posneg pres province seq-easy \
	seq-hard sexes sov state trek ucc

beforeinstall:
	(cd ${.CURDIR}/datfiles; ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} \
	    -g ${BINGRP} -m 444 ${CATS} ${DESTDIR}/usr/share/games/quiz.db)

.include <bsd.prog.mk>
