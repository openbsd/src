#	$OpenBSD: Makefile,v 1.11 2004/02/01 07:21:56 drahn Exp $

.if make(obj)
SUBDIR= alpha arm hppa i386 m68k m88k powerpc sparc sparc64 vax x86_64
.else
SUBDIR= ${MACHINE_ARCH}
.endif

.include <bsd.subdir.mk>
