#	$OpenBSD: Makefile,v 1.7 2004/08/15 21:43:35 deraadt Exp $

SUBDIR= ramdisk_cd ramdiskA ramdiskB ramdiskC cdfs cdfs-emu

unconfig:
	cd ramdisk_cd; ${MAKE} unconfig
	cd ramdiskA; ${MAKE} unconfig
	cd ramdiskB; ${MAKE} unconfig
	cd ramdiskC; ${MAKE} unconfig

.include <bsd.subdir.mk>
