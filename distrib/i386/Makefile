#	$OpenBSD: Makefile,v 1.8 2007/08/14 18:57:32 deraadt Exp $

SUBDIR= ramdisk_cd ramdiskA ramdiskB ramdiskC cdfs cdfs-emu

.if	make(obj)
SUBDIR+= iso
.endif

unconfig:
	cd ramdisk_cd; ${MAKE} unconfig
	cd ramdiskA; ${MAKE} unconfig
	cd ramdiskB; ${MAKE} unconfig
	cd ramdiskC; ${MAKE} unconfig

.include <bsd.subdir.mk>
