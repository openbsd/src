#	$OpenBSD: Makefile,v 1.6 2016/05/11 21:52:49 deraadt Exp $

PROG=copy
CFLAGS+= -Wall

# Distinct address space:	hppa m88k powerpc sparc64
# Shared address space:		alpha amd64 arm i386 mips sparc vax

.if (${MACHINE_ARCH} == "hppa" || ${MACHINE_ARCH} == "m88k" || \
    ${MACHINE_ARCH} == "powerpc" || ${MACHINE_ARCH} == "sparc64")
REGRESS_SKIP=
.endif

.include <bsd.regress.mk>
