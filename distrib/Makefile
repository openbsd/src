#	$OpenBSD: Makefile,v 1.43 2013/09/04 20:00:21 patrick Exp $

SUBDIR=	special notes

.if	make(obj)
SUBDIR+=alpha amd64 armish armv7 hp300 hppa i386 landisk loongson luna88k \
	macppc mvme68k mvme88k octeon sgi socppc sparc sparc64 vax zaurus
.elif exists(${MACHINE})
SUBDIR+=${MACHINE}
.endif

.include <bsd.subdir.mk>
