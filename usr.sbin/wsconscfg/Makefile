#	$OpenBSD: Makefile,v 1.12 2004/01/28 17:42:59 deraadt Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "amd64" || \
    ${MACHINE} == "alpha" || ${MACHINE} == "hppa" || \
    ${MACHINE} == "macppc" || ${MACHINE} == "sparc" || \
    ${MACHINE} == "sparc64" || ${MACHINE} == "pegasos"

PROG=	wsconscfg
SRCS=	wsconscfg.c

.else

NOPROG=yes

.endif

MAN=	wsconscfg.8
MANSUBDIR=	alpha hppa i386 macppc pegasos sparc sparc64 amd64

.include <bsd.prog.mk>
