#	$OpenBSD: Makefile,v 1.14 2006/11/09 15:19:18 drahn Exp $

.if make(obj)
SUBDIR= alpha arm hppa i386 m68k m88k powerpc sh sparc sparc64 vax amd64 mips64
.else
SUBDIR= ${MACHINE_ARCH}
.endif

.include <bsd.subdir.mk>
