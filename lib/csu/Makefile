#	$OpenBSD: Makefile,v 1.5 2001/07/11 03:02:26 pvalchev Exp $

.if make(obj)
SUBDIR= alpha i386 m68k m88k ns32k powerpc sparc vax
.else
SUBDIR= ${MACHINE_ARCH}
.endif

SUBDIR+= c++

.include <bsd.subdir.mk>
