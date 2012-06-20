#	$OpenBSD: Makefile,v 1.40 2012/06/20 18:33:58 matthew Exp $

SUBDIR=	special notes

.if	make(obj)
SUBDIR+=alpha amd64 armish beagle hp300 hppa i386 landisk loongson macppc \
	mvme68k mvme88k sgi socppc sparc sparc64 vax zaurus
.elif exists(${MACHINE})
SUBDIR+=${MACHINE}
.endif

.include <bsd.subdir.mk>
