#	$OpenBSD: Makefile,v 1.2 2004/02/22 00:24:41 deraadt Exp $

SUBDIR= ramdisk_cd ramdiskA cdfs

unconfig:
	cd ramdisk_cd; ${MAKE} unconfig
	cd ramdiskA; ${MAKE} unconfig

.include <bsd.subdir.mk>
