#	$OpenBSD: Makefile,v 1.3 2011/07/08 15:20:40 nicm Exp $
#	from: @(#)Makefile	5.3 (Berkeley) 10/22/90

FILES=	std stdcrt vt100 vt300
NOOBJ=	noobj

all clean cleandir depend lint tags:

realinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/tabset

.include <bsd.prog.mk>
