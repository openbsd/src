#	$OpenBSD: Makefile,v 1.5 2017/06/12 18:49:33 espie Exp $

NOOBJ=	noobj
all wargames clean cleandir depend tags:

install:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/wargames.sh ${DESTDIR}/usr/games/wargames

.include <bsd.prog.mk>
