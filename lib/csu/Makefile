#	$OpenBSD: Makefile,v 1.6 2001/08/19 19:58:43 mickey Exp $

.if make(obj)
SUBDIR= alpha hppa i386 m68k m88k ns32k powerpc sparc vax
.else
SUBDIR= ${MACHINE_ARCH}
.endif

SUBDIR+= c++

.include <bsd.subdir.mk>
