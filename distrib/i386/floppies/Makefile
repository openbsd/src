#	$OpenBSD: Makefile,v 1.8 1999/08/06 20:49:05 deraadt Exp $

SUBDIR= ramdisk_cd ramdisk ramdiskB

unconfig:
	cd ramdisk_cd; ${MAKE} unconfig
	cd ramdisk; ${MAKE} unconfig
	cd ramdiskB; ${MAKE} unconfig

.include <bsd.subdir.mk>
