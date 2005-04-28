#	$OpenBSD: Makefile,v 1.3 2005/04/28 11:21:18 otto Exp $

PROG=copy
CFLAGS+= -Wall

# Distinct address space:	hppa hppa64 m68k m88k powerpc sparc64
# Shared address space:		alpha amd64 arm i386 mips solbourne sparc vax

.if (${MACHINE_ARCH} == "hppa" || ${MACHINE_ARCH} == "hppa64" || \
    ${MACHINE_ARCH} == "m68k" || ${MACHINE_ARCH} == "m88k" || \
    ${MACHINE_ARCH} == "powerpc" || ${MACHINE_ARCH} == "sparc64")
REGRESS_SKIP=
.endif

.include <bsd.regress.mk>
