#	$OpenBSD: Makefile,v 1.42 2013/03/26 18:06:00 jasper Exp $

SUBDIR=	special notes

.if	make(obj)
SUBDIR+=alpha amd64 armish beagle hp300 hppa i386 landisk loongson luna88k \
	macppc mvme68k mvme88k octeon sgi socppc sparc sparc64 vax zaurus
.elif exists(${MACHINE})
SUBDIR+=${MACHINE}
.endif

.include <bsd.subdir.mk>
