#	$OpenBSD: Makefile,v 1.6 2000/04/28 23:17:04 espie Exp $
#	$NetBSD: Makefile,v 1.5 1995/09/15 21:05:21 pk Exp $

.if make(obj)
SUBDIR=  arch/alpha arch/amiga arch/arc arch/hp300 arch/i386 \
	arch/m68k arch/mac68k arch/mvme68k arch/pmax arch/powerpc \
	arch/sparc arch/wgrisc arch/vax
.else
SUBDIR+= arch/${MACHINE}

# For manpages
.if ${MACHINE} != "amiga"
SUBDIR+= arch/amiga
.endif

.endif

.include <bsd.subdir.mk>
