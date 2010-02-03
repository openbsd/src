#	$OpenBSD: Makefile,v 1.15 2010/02/03 20:49:00 miod Exp $

.if make(obj)
SUBDIR= alpha amd64 arm hppa i386 m68k m88k mips64 powerpc sh sparc sparc64 vax
.else
SUBDIR= ${MACHINE_CPU}
.endif

.include <bsd.subdir.mk>
