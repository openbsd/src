#	$Id: Makefile,v 1.2 1997/11/08 08:13:10 todd Exp $

.if make(obj)
SUBDIR= alpha i386 m68k mips mvme88k ns32k powerpc sparc vax
.else
SUBDIR= ${MACHINE_ARCH}
.endif

SUBDIR+= c++

.include <bsd.subdir.mk>
