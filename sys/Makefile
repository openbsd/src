#	$OpenBSD: Makefile,v 1.3 1997/10/20 00:33:52 deraadt Exp $
#	$NetBSD: Makefile,v 1.5 1995/09/15 21:05:21 pk Exp $

.if make(obj)
SUBDIR=  arch/alpha arch/amiga arch/arc arch/hp300 arch/i386 \
	arch/m68k arch/mac68k arch/mvme68k arch/pmax arch/powerpc \
	arch/sparc arch/wgrisc
.else
SUBDIR+= arch/${MACHINE}
.endif

.include <bsd.subdir.mk>
