#	$OpenBSD: Makefile,v 1.7 2001/08/19 20:44:31 brad Exp $

.if make(obj)
SUBDIR= alpha hppa i386 m68k m88k powerpc sparc vax
.else
SUBDIR= ${MACHINE_ARCH}
.endif

SUBDIR+= c++

.include <bsd.subdir.mk>
