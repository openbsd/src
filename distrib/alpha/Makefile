#	$OpenBSD: Makefile,v 1.11 2017/01/27 17:59:09 natano Exp $

SUBDIR= bsd.rd miniroot floppy floppyB floppyC cdfs

.if	make(obj) || make(cleandir) || make(clean)
SUBDIR+= iso
.endif

unconfig:
	cd miniroot; ${MAKE} unconfig
	cd floppy; ${MAKE} unconfig
	cd floppyB; ${MAKE} unconfig
	cd floppyC; ${MAKE} unconfig
	cd cdfs; ${MAKE} unconfig
	cd iso; ${MAKE} unconfig

.include <bsd.subdir.mk>
