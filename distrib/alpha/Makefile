#	$OpenBSD: Makefile,v 1.6 2003/03/06 00:02:05 deraadt Exp $

SUBDIR= bsd.rd floppy floppyB cdfs # floppyC

unconfig:
	cd bsd.rd; ${MAKE} unconfig
	cd floppy; ${MAKE} unconfig
	cd floppyB; ${MAKE} unconfig
	cd cdfs; ${MAKE} unconfig
#	cd floppyC; ${MAKE} unconfig

.include <bsd.subdir.mk>
