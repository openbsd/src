#	$OpenBSD: Makefile,v 1.5 2000/04/11 20:13:11 millert Exp $
#	from: @(#)Makefile	6.13 (Berkeley) 3/20/91

BINDIR= /usr/share
FILES=	tmac.andoc
LINKS=	${BINDIR}/tmac/tmac.andoc ${BINDIR}/tmac/tmac.an
NOOBJ=	noobj

SUBDIR=	mdoc

all clean cleandir depend lint tags:

realinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
		${DESTDIR}${BINDIR}/tmac

.include <bsd.prog.mk>
