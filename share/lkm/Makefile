#	$OpenBSD: Makefile,v 1.4 2005/11/24 12:08:15 pedro Exp $
#
#

FILES=	README syscall misc
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	find ${FILES} -follow -name CVS -prune -o -type f -print | \
	    cpio -pdumL ${DESTDIR}${BINDIR}/lkm
	chown -R ${BINOWN}:${BINGRP} ${DESTDIR}${BINDIR}/lkm
	find ${DESTDIR}${BINDIR}/lkm -type f -print0 | xargs -0r chmod a=rX
	find ${DESTDIR}${BINDIR}/lkm -type d -print0 | xargs -0r chmod u=rwX,go=rX

.include <bsd.prog.mk>
