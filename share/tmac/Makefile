#	$OpenBSD: Makefile,v 1.3 2000/03/02 14:46:54 todd Exp $
#	from: @(#)Makefile	6.13 (Berkeley) 3/20/91

BINDIR= /usr/share
FILES=	tmac.andoc
LINKS=	${BINDIR}/tmac/tmac.andoc ${BINDIR}/tmac/tmac.an
NOOBJ=	noobj

SUBDIR=	mdoc

# old macros not currently installed...
OFILES=	tmac.an.v6compat tmac.an6n tmac.an6t tmac.a tmac.cp tmac.doc.old

all clean cleandir depend lint tags:

realinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
		${DESTDIR}${BINDIR}/tmac

.include <bsd.prog.mk>
