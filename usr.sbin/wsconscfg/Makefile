#	$OpenBSD: Makefile,v 1.11 2003/10/31 04:12:20 drahn Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "alpha" || ${MACHINE} == "hppa" || \
    ${MACHINE} == "macppc" || ${MACHINE} == "sparc" || \
    ${MACHINE} == "sparc64" || ${MACHINE} == "pegasos"

PROG=	wsconscfg
SRCS=	wsconscfg.c

.else

NOPROG=yes

.endif

MAN=	wsconscfg.8
MANSUBDIR=	alpha hppa i386 macppc pegasos sparc sparc64

.include <bsd.prog.mk>
