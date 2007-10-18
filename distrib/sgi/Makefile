#	$OpenBSD: Makefile,v 1.4 2007/10/18 18:16:20 deraadt Exp $

SUBDIR= ramdisk cdfs

.if	make(obj) || make(cleandir) || make(clean)
SUBDIR+= iso
.endif

.include <bsd.subdir.mk>
