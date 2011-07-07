#	$OpenBSD: Makefile,v 1.39 2011/07/07 19:16:43 deraadt Exp $

SUBDIR=	special notes

.if	make(obj)
SUBDIR+=alpha amd64 armish beagle hp300 hppa i386 landisk loongson mac68k macppc \
	mvme68k mvme88k sgi socppc sparc sparc64 vax zaurus
.elif exists(${MACHINE})
SUBDIR+=${MACHINE}
.endif

.include <bsd.subdir.mk>
