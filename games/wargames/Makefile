#	$OpenBSD: Makefile,v 1.4 1997/09/21 11:37:12 deraadt Exp $

NOOBJ=	noobj
all wargames clean cleandir depend lint tags:

install:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/wargames.sh ${DESTDIR}/usr/games/wargames

.include <bsd.prog.mk>
