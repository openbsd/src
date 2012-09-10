#	$OpenBSD: Makefile,v 1.10 2012/09/10 22:21:03 deraadt Exp $

# preserve order: 'miniroot' and 'cdfs' depend on 'ramdisk'
SUBDIR+= ../ramdisk miniroot cdfs

.if	make(obj) || make(cleandir) || make(clean)
SUBDIR+= iso
.endif

unconfig:
	cd ../ramdisk; ${MAKE} unconfig

.include <bsd.subdir.mk>
