#	$OpenBSD: Makefile,v 1.5 2015/12/01 08:13:30 deraadt Exp $

PROG=copy
CFLAGS+= -Wall

# Distinct address space:	hppa hppa64 m88k powerpc sparc64
# Shared address space:		alpha amd64 arm i386 mips sparc vax

.if (${MACHINE_ARCH} == "hppa" || ${MACHINE_ARCH} == "hppa64" || \
    ${MACHINE_ARCH} == "m88k" || \
    ${MACHINE_ARCH} == "powerpc" || ${MACHINE_ARCH} == "sparc64")
REGRESS_SKIP=
.endif

.include <bsd.regress.mk>
