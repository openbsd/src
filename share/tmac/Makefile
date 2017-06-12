#	$OpenBSD: Makefile,v 1.7 2017/06/12 18:49:34 espie Exp $
#	from: @(#)Makefile	6.13 (Berkeley) 3/20/91

FILES=	tmac.andoc
LINKS=	${BINDIR}/tmac/tmac.andoc ${BINDIR}/tmac/tmac.an
NOOBJ=	noobj

SUBDIR=	mdoc

all clean cleandir depend tags:

realinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
		${DESTDIR}${BINDIR}/tmac

.include <bsd.prog.mk>
