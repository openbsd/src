#	$OpenBSD: Makefile,v 1.13 2004/09/10 12:07:26 pefo Exp $

.if make(obj)
SUBDIR= alpha arm hppa i386 m68k m88k powerpc sparc sparc64 vax amd64 mips64
.else
SUBDIR= ${MACHINE_ARCH}
.endif

.include <bsd.subdir.mk>
