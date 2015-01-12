#	$OpenBSD: Makefile,v 1.11 2015/01/12 17:10:36 deraadt Exp $

SUBDIR= ramdisk_cd ramdisk

.if	make(obj) || make(cleandir) || make(clean)
SUBDIR+= iso
.endif

unconfig:
	cd ramdisk_cd; ${MAKE} unconfig
	cd ramdisk; ${MAKE} unconfig

.include <bsd.subdir.mk>
