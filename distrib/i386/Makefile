#	$OpenBSD: Makefile,v 1.5 2002/04/22 02:40:02 deraadt Exp $

SUBDIR= ramdisk_cd ramdiskA ramdiskB ramdiskC

unconfig:
	cd ramdisk_cd; ${MAKE} unconfig
	cd ramdiskA; ${MAKE} unconfig
	cd ramdiskB; ${MAKE} unconfig
	cd ramdiskC; ${MAKE} unconfig

.include <bsd.subdir.mk>
