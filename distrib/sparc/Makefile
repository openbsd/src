#	$OpenBSD: Makefile,v 1.7 2007/08/14 18:57:32 deraadt Exp $

SUBDIR+= ../ramdisk ../miniroot cdfs

.if	make(obj)
SUBDIR+= iso
.endif

unconfig:
	cd ramdisk_cd; ${MAKE} unconfig
	cd ramdiskA; ${MAKE} unconfig

.include <bsd.subdir.mk>
