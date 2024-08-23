#	$OpenBSD: Makefile,v 1.9 2024/08/23 17:29:08 deraadt Exp $

PROG=	quiz
MAN=	quiz.6
SRCS=	quiz.c rxp.c
CATS=	africa america arith asia babies bard chinese collectives \
	ed elements europe flowers ship inca index latin locomotive \
	midearth morse mult murders poetry posneg pres province seq-easy \
	seq-hard sexes sov state trek ucc

NAPHONE= ${.CURDIR}/../../share/misc/na.phone
CLEANFILES+= areas

areas: ${NAPHONE}
	sh ${.CURDIR}/naphone2areas.sh ${NAPHONE} > areas

afterinstall: areas
	(cd ${.CURDIR}/datfiles; ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} \
	    -g ${BINGRP} -m 444 ${CATS} ${DESTDIR}/usr/share/games/quiz.db)
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    areas ${DESTDIR}/usr/share/games/quiz.db

.include <bsd.prog.mk>
