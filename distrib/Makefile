#	$OpenBSD: Makefile,v 1.9 1998/12/17 02:16:29 smurph Exp $

.if	make(obj)
SUBDIR= special alpha amiga arc crunch hp300 i386 mac68k mvme68k mvme88k\
	pmax powerpc sparc
.elif	!make(install)
SUBDIR= special
.endif
.if exists(${MACHINE})
SUBDIR+= ${MACHINE}
.endif
SUBDIR+= notes

.include <bsd.subdir.mk>
