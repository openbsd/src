#	$OpenBSD: Makefile,v 1.5 2001/01/29 21:25:45 deraadt Exp $

SUBDIR= bsd.rd floppy floppyB # floppyC

unconfig:
	cd bsd.rd; ${MAKE} unconfig
	cd floppy; ${MAKE} unconfig
	cd floppyB; ${MAKE} unconfig
#	cd floppyC; ${MAKE} unconfig

.include <bsd.subdir.mk>
