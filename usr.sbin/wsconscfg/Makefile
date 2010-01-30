#	$OpenBSD: Makefile,v 1.31 2010/01/30 20:48:53 miod Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "amd64" || \
    ${MACHINE} == "armish" || ${MACHINE} == "hp300" || \
    ${MACHINE} == "hppa" || ${MACHINE} == "hppa64" || \
    ${MACHINE} == "i386" || ${MACHINE} == "landisk" || \
    ${MACHINE} == "loongson" || ${MACHINE} == "luna88k" || \
    ${MACHINE} == "mac68k" || ${MACHINE} == "macppc" || \
    ${MACHINE} == "palm" || ${MACHINE} == "sgi" || \
    ${MACHINE} == "socppc" || ${MACHINE} == "sparc" || \
    ${MACHINE} == "sparc64" || ${MACHINE} == "vax" || \
    ${MACHINE} == "zaurus"

PROG=	wsconscfg
SRCS=	wsconscfg.c

.else

NOPROG=yes

.endif

MAN=	wsconscfg.8

.include <bsd.prog.mk>
