#	$OpenBSD: Makefile,v 1.10 2014/03/02 20:19:17 deraadt Exp $

SUBDIR= ramdisk_cd ramdiskA ramdiskB ramdiskC cdfs

.if	make(obj) || make(cleandir) || make(clean)
SUBDIR+= iso
.endif

unconfig:
	cd ramdisk_cd; ${MAKE} unconfig
	cd ramdiskA; ${MAKE} unconfig
	cd ramdiskB; ${MAKE} unconfig
	cd ramdiskC; ${MAKE} unconfig

.include <bsd.subdir.mk>
