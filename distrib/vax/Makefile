#	$OpenBSD: Makefile,v 1.5 2008/02/25 19:52:03 deraadt Exp $

SUBDIR=	../ramdisk cdfs

.if	make(obj) || make(cleandir) || make(clean)
SUBDIR+= iso
.endif

.include <bsd.subdir.mk>
