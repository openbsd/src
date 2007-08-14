#	$OpenBSD: Makefile,v 1.7 2007/08/14 18:57:32 deraadt Exp $

SUBDIR= bsd.rd floppy floppyB cdfs # floppyC

.if	make(obj)
SUBDIR+= iso
.endif

unconfig:
	cd bsd.rd; ${MAKE} unconfig
	cd floppy; ${MAKE} unconfig
	cd floppyB; ${MAKE} unconfig
	cd cdfs; ${MAKE} unconfig
#	cd floppyC; ${MAKE} unconfig

.include <bsd.subdir.mk>
