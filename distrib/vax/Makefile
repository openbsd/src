#	$OpenBSD: Makefile,v 1.4 2008/02/24 11:53:10 maja Exp $

SUBDIR=	../ramdisk

.if	make(obj) || make(cleandir) || make(clean)
SUBDIR+= iso
.endif

.include <bsd.subdir.mk>
