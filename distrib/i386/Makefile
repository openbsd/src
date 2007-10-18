#	$OpenBSD: Makefile,v 1.9 2007/10/18 18:16:20 deraadt Exp $

SUBDIR= ramdisk_cd ramdiskA ramdiskB ramdiskC cdfs cdfs-emu

.if	make(obj) || make(cleandir) || make(clean)
SUBDIR+= iso
.endif

unconfig:
	cd ramdisk_cd; ${MAKE} unconfig
	cd ramdiskA; ${MAKE} unconfig
	cd ramdiskB; ${MAKE} unconfig
	cd ramdiskC; ${MAKE} unconfig

.include <bsd.subdir.mk>
