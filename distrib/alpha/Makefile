#	$OpenBSD: Makefile,v 1.10 2015/08/29 16:48:22 deraadt Exp $

SUBDIR= bsd.rd miniroot floppy floppyB floppyC cdfs

.if	make(obj) || make(cleandir) || make(clean)
SUBDIR+= iso
.endif

unconfig:
	cd bsd.rd; ${MAKE} unconfig
	cd miniroot; ${MAKE} unconfig
	cd floppy; ${MAKE} unconfig
	cd floppyB; ${MAKE} unconfig
	cd floppyC; ${MAKE} unconfig
	cd cdfs; ${MAKE} unconfig

.include <bsd.subdir.mk>
