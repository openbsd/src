#	$OpenBSD: Makefile,v 1.10 2002/08/12 10:47:43 miod Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "alpha" || ${MACHINE} == "hppa" || \
    ${MACHINE} == "macppc" || ${MACHINE} == "sparc" || ${MACHINE} == "sparc64"

PROG=	wsconscfg
SRCS=	wsconscfg.c

.else

NOPROG=yes

.endif

MAN=	wsconscfg.8
MANSUBDIR=	alpha hppa i386 macppc sparc sparc64

.include <bsd.prog.mk>
