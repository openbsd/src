#	$OpenBSD: Makefile,v 1.2 1997/05/29 13:48:09 grr Exp $

SUBDIR=	${MACHINE}

# miniroot and ramdisk are supposed to be "generic" using Makefile.inc
# to set up architectural dependencies..
.if	(${MACHINE} == "sparc")
SUBDIR+=        ramdisk miniroot
.endif

.include <bsd.subdir.mk>
