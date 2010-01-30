#	$OpenBSD: Makefile,v 1.4 2010/01/30 20:48:53 miod Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "amd64" || \
    ${MACHINE} == "armish" || ${MACHINE} == "hppa" || \
    ${MACHINE} == "hppa64" || ${MACHINE} == "i386" || \
    ${MACHINE} == "landisk" || ${MACHINE} == "loongson" || \
    ${MACHINE} == "macppc" || ${MACHINE} == "sgi" || \
    ${MACHINE} == "socppc" || ${MACHINE} == "sparc64"

PROG=	pcidump

.else
NOPROG=yes
.endif

MAN=	pcidump.8

.include <bsd.prog.mk>
