#	$OpenBSD: Makefile,v 1.9 2004/01/04 17:26:37 drahn Exp $

.if make(obj)
SUBDIR= alpha hppa i386 m68k m88k powerpc sparc sparc64 vax
.else
SUBDIR= ${MACHINE_ARCH}
.endif

.include <bsd.subdir.mk>
