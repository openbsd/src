#	$OpenBSD: Makefile,v 1.4 1997/09/17 14:18:58 deraadt Exp $

SUBDIR=	special ${MACHINE} notes

# miniroot and ramdisk are supposed to be "generic" using Makefile.inc
# to set up architectural dependencies..
.if	(${MACHINE} == "sparc")
SUBDIR+=        ramdisk miniroot
.endif

.include <bsd.subdir.mk>
