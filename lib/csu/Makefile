#	$OpenBSD: Makefile,v 1.3 1999/08/20 14:11:34 niklas Exp $

.if make(obj)
SUBDIR= alpha i386 m68k mips mvme88k ns32k powerpc sparc vax
.else
SUBDIR= ${MACHINE_ARCH}
.endif

SUBDIR+= c++

.include <bsd.subdir.mk>
