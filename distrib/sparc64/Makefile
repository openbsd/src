#	$OpenBSD: Makefile,v 1.10 2012/06/22 02:56:42 deraadt Exp $

# preserve order: 'miniroot' depends on 'bsd.rd', 'cdfs' depends on 'miniroot'
SUBDIR+= ramdisk ramdiskB bsd.rd miniroot cdfs

.if	make(obj) || make(cleandir) || make(clean)
SUBDIR+= iso
.endif

unconfig:
	cd ramdisk; ${MAKE} unconfig
	cd ramdiskB; ${MAKE} unconfig

.include <bsd.subdir.mk>
