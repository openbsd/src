#	$OpenBSD: Makefile,v 1.5 2000/02/12 15:31:08 espie Exp $
#	$NetBSD: Makefile,v 1.5 1995/09/15 21:05:21 pk Exp $

.if make(obj)
SUBDIR=  arch/alpha arch/arc arch/hp300 arch/i386 \
	arch/m68k arch/mac68k arch/mvme68k arch/pmax arch/powerpc \
	arch/sparc arch/wgrisc arch/vax
.else
SUBDIR+= arch/${MACHINE}
.endif

# For manpages
.if ${MACHINE} != "amiga"
SUBDIR+= arch/amiga
.endif

.include <bsd.subdir.mk>
