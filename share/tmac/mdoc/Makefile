#	$OpenBSD: Makefile,v 1.4 2003/04/08 01:44:59 millert Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/8/93

MSRCS=	doc doc-ditroff doc-common doc-nroff doc-syms
BINDIR=	/usr/share/tmac/mdoc
LINKS=	${BINDIR}/doc ${BINDIR}/../tmac.doc

STRIP_SED = ${.CURDIR}/../strip.sed
NOOBJ=	noobj

all clean cleandir depend lint tags:

beforeinstall:
	for i in ${MSRCS}; do \
		sed -f ${STRIP_SED} < $$i > ${DESTDIR}${BINDIR}/$$i; \
		chown ${BINOWN}:${BINGRP} ${DESTDIR}${BINDIR}/$$i; \
		chmod 444 ${DESTDIR}${BINDIR}/$$i; \
	done

.include <bsd.prog.mk>
