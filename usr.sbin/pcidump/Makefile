#	$OpenBSD: Makefile,v 1.16 2026/07/15 19:00:24 miod Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "amd64" || \
    ${MACHINE} == "arm64" || ${MACHINE} == "armv7" || \
    ${MACHINE} == "hppa" || ${MACHINE} == "i386" || \
    ${MACHINE} == "landisk" || \
    ${MACHINE} == "macppc" || ${MACHINE} == "octeon" || \
    ${MACHINE} == "powerpc64" || ${MACHINE} == "riscv64" || \
    ${MACHINE} == "sparc64"

PROG=	pcidump

.else
NOPROG=yes
.endif

MAN=	pcidump.8

.include <bsd.prog.mk>
