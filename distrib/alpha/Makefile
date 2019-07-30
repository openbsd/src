#	$OpenBSD: Makefile,v 1.13 2017/09/17 16:31:59 deraadt Exp $

SUBDIR= bsd.rd miniroot cdfs

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
