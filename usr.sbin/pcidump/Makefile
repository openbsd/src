#	$OpenBSD: Makefile,v 1.9 2017/02/26 04:06:38 jsg Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "amd64" || \
    ${MACHINE} == "arm64" || ${MACHINE} == "hppa" || \
    ${MACHINE} == "i386" || ${MACHINE} == "landisk" || \
    ${MACHINE} == "loongson" || ${MACHINE} == "macppc" || \
    ${MACHINE} == "octeon" || ${MACHINE} == "sgi" || \
    ${MACHINE} == "socppc" || ${MACHINE} == "sparc64"

PROG=	pcidump

.else
NOPROG=yes
.endif

MAN=	pcidump.8

.include <bsd.prog.mk>
