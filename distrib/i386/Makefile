#	$OpenBSD: Makefile,v 1.12 2015/02/23 09:33:38 deraadt Exp $

SUBDIR= ramdisk_cd ramdisk cdfs

.if	make(obj) || make(cleandir) || make(clean)
SUBDIR+= iso
.endif

unconfig:
	cd ramdisk_cd; ${MAKE} unconfig
	cd ramdisk; ${MAKE} unconfig

.include <bsd.subdir.mk>
