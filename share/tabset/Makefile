#	$OpenBSD: Makefile,v 1.5 2019/12/21 21:40:01 espie Exp $
#	from: @(#)Makefile	5.3 (Berkeley) 10/22/90

FILES=	std stdcrt vt100 vt300
NOOBJ=	noobj

all clean cleandir tags:

realinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/tabset

.include <bsd.prog.mk>
