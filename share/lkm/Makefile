#	$OpenBSD: Makefile,v 1.3 2005/09/27 16:27:23 deraadt Exp $
#
#

FILES=	README syscall vfs misc
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	find ${FILES} -follow -name CVS -prune -o -type f -print | \
	    cpio -pdumL ${DESTDIR}${BINDIR}/lkm
	chown -R ${BINOWN}:${BINGRP} ${DESTDIR}${BINDIR}/lkm
	find ${DESTDIR}${BINDIR}/lkm -type f -print0 | xargs -0r chmod a=rX
	find ${DESTDIR}${BINDIR}/lkm -type d -print0 | xargs -0r chmod u=rwX,go=rX

.include <bsd.prog.mk>
