#	$OpenBSD: Makefile,v 1.8 2007/08/14 18:57:32 deraadt Exp $

# preserve order: 'miniroot' depends on 'bsd.rd', 'cdfs' depends on 'miniroot'
SUBDIR+= ramdisk ramdiskB bsd.rd miniroot cdfs

.if	make(obj)
SUBDIR+= iso
.endif

unconfig:
	cd ramdisk_cd; ${MAKE} unconfig
	cd ramdiskA; ${MAKE} unconfig

.include <bsd.subdir.mk>
