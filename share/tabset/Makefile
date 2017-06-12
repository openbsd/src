#	$OpenBSD: Makefile,v 1.4 2017/06/12 18:49:34 espie Exp $
#	from: @(#)Makefile	5.3 (Berkeley) 10/22/90

FILES=	std stdcrt vt100 vt300
NOOBJ=	noobj

all clean cleandir depend tags:

realinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/tabset

.include <bsd.prog.mk>
