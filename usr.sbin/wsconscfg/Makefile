#	$OpenBSD: Makefile,v 1.17 2005/01/13 19:03:56 drahn Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "amd64" || \
    ${MACHINE_ARCH} == "arm" || ${MACHINE} == "hppa" || \
    ${MACHINE} == "i386" || ${MACHINE} == "luna88k" || \
    ${MACHINE} == "macppc" || ${MACHINE} == "sparc" || \
    ${MACHINE} == "sparc64" || ${MACHINE} == "sgi"

PROG=	wsconscfg
SRCS=	wsconscfg.c

.else

NOPROG=yes

.endif

MAN=	wsconscfg.8
MANSUBDIR=	alpha amd64 cats hppa i386 luna88k macppc sparc sparc64 sgi

.include <bsd.prog.mk>
