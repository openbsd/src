#	$OpenBSD: Makefile,v 1.8 2005/01/13 19:03:56 drahn Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "amd64" || \
    ${MACHINE} == "alpha" || ${MACHINE} == "hppa" || \
    ${MACHINE_ARCH} == "arm"

PROG=	wsfontload
SRCS=	wsfontload.c

.else

NOPROG=yes

.endif

MAN=	wsfontload.8
MANSUBDIR=	alpha hppa i386 amd64 cats

.include <bsd.prog.mk>
