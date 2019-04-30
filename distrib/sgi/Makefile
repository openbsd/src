#	$OpenBSD: Makefile,v 1.5 2019/04/30 17:46:05 deraadt Exp $

SUBDIR= ramdisk cdfs

.if	make(obj) || make(cleandir) || make(clean)
SUBDIR+= iso
.endif

unconfig:
	cd ramdisk; ${MAKE} unconfig

.include <bsd.subdir.mk>
