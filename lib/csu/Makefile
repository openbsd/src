#	$OpenBSD: Makefile,v 1.12 2004/02/27 17:36:14 deraadt Exp $

.if make(obj)
SUBDIR= alpha arm hppa i386 m68k m88k powerpc sparc sparc64 vax amd64
.else
SUBDIR= ${MACHINE_ARCH}
.endif

.include <bsd.subdir.mk>
