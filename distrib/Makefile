#	$OpenBSD: Makefile,v 1.37 2010/02/03 21:47:09 otto Exp $

SUBDIR=	special notes

.if	make(obj)
SUBDIR+=alpha amd64 armish hp300 hppa i386 landisk loongson mac68k macppc \
	mvme68k mvme88k mvmeppc sgi socppc sparc sparc64 vax zaurus
.elif exists(${MACHINE})
SUBDIR+=${MACHINE}
.endif

.include <bsd.subdir.mk>
