#	$OpenBSD: Makefile,v 1.13 2020/06/28 19:11:53 drahn Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "amd64" || \
    ${MACHINE} == "arm64" || ${MACHINE} == "armv7" || \
    ${MACHINE} == "hppa" || ${MACHINE} == "i386" || \
    ${MACHINE} == "landisk" || ${MACHINE} == "loongson" || \
    ${MACHINE} == "macppc" || ${MACHINE} == "octeon" || \
    ${MACHINE} == "sgi" || ${MACHINE} == "sparc64" || \
    ${MACHINE} == "powerpc64"

PROG=	pcidump

.else
NOPROG=yes
.endif

MAN=	pcidump.8

.include <bsd.prog.mk>
