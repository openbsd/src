#	$OpenBSD: Makefile,v 1.9 2009/04/17 03:58:55 deraadt Exp $

SUBDIR+= ../ramdisk ../miniroot cdfs

.if	make(obj) || make(cleandir) || make(clean)
SUBDIR+= iso
.endif

unconfig:
	cd ramdisk; ${MAKE} unconfig
	cd ramdiskA; ${MAKE} unconfig

.include <bsd.subdir.mk>
