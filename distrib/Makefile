#	$OpenBSD: Makefile,v 1.3 1997/08/06 09:40:30 deraadt Exp $

SUBDIR=	${MACHINE} notes

# miniroot and ramdisk are supposed to be "generic" using Makefile.inc
# to set up architectural dependencies..
.if	(${MACHINE} == "sparc")
SUBDIR+=        ramdisk miniroot
.endif

.include <bsd.subdir.mk>
