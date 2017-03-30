#	$OpenBSD: Makefile,v 1.12 2017/03/30 17:29:17 deraadt Exp $

SUBDIR= bsd.rd miniroot floppy floppyC cdfs

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
