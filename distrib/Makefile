#	$OpenBSD: Makefile,v 1.38 2011/03/22 17:35:01 deraadt Exp $

SUBDIR=	special notes

.if	make(obj)
SUBDIR+=alpha amd64 armish beagle hp300 hppa i386 landisk loongson mac68k macppc \
	mvme68k mvme88k mvmeppc sgi socppc sparc sparc64 vax zaurus
.elif exists(${MACHINE})
SUBDIR+=${MACHINE}
.endif

.include <bsd.subdir.mk>
