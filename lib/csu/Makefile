#	$OpenBSD: Makefile,v 1.8 2001/09/11 01:28:40 jason Exp $

.if make(obj)
SUBDIR= alpha hppa i386 m68k m88k powerpc sparc sparc64 vax
.else
SUBDIR= ${MACHINE_ARCH}
.endif

SUBDIR+= c++

.include <bsd.subdir.mk>
