#	$OpenBSD: Makefile,v 1.2 2000/03/02 14:46:35 todd Exp $
#
#

FILES=	README syscall vfs misc
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	find ${FILES} -follow -name CVS -prune -o -type f -print | \
	    cpio -pdumL ${DESTDIR}${BINDIR}/lkm
	chown -R ${BINOWN}:${BINGRP} ${DESTDIR}${BINDIR}/lkm
	find ${DESTDIR}${BINDIR}/lkm -type f | xargs chmod a=rX
	find ${DESTDIR}${BINDIR}/lkm -type d | xargs chmod u=rwX,go=rX

.include <bsd.prog.mk>
