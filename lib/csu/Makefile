#	$OpenBSD: Makefile,v 1.4 2001/06/26 22:42:49 miod Exp $

.if make(obj)
SUBDIR= alpha i386 m68k mips m88k ns32k powerpc sparc vax
.else
SUBDIR= ${MACHINE_ARCH}
.endif

SUBDIR+= c++

.include <bsd.subdir.mk>
