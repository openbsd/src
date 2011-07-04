#	$OpenBSD: Makefile,v 1.16 2011/07/04 05:54:27 deraadt Exp $

.if make(obj)
SUBDIR= alpha amd64 arm hppa i386 ia64 m68k m88k mips64 powerpc sh sparc sparc64 vax
.else
SUBDIR= ${MACHINE_CPU}
.endif

.include <bsd.subdir.mk>
