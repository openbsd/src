#	$OpenBSD: Makefile,v 1.9 2000/07/11 19:23:58 deraadt Exp $

SUBDIR= ramdisk_cd ramdisk ramdiskB ramdiskC

unconfig:
	cd ramdisk_cd; ${MAKE} unconfig
	cd ramdisk; ${MAKE} unconfig
	cd ramdiskB; ${MAKE} unconfig
	cd ramdiskC; ${MAKE} unconfig

.include <bsd.subdir.mk>
