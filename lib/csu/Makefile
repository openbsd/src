#	$OpenBSD: Makefile,v 1.10 2004/01/28 17:37:46 deraadt Exp $

.if make(obj)
SUBDIR= alpha hppa i386 m68k m88k powerpc sparc sparc64 vax x86_64
.else
SUBDIR= ${MACHINE_ARCH}
.endif

.include <bsd.subdir.mk>
