#	$OpenBSD: Makefile,v 1.9 2005/01/13 19:21:04 drahn Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "amd64" || \
    ${MACHINE} == "alpha" || ${MACHINE} == "hppa" || \
    ${MACHINE} == "cats" || ${MACHINE} == "zaurus"

PROG=	wsfontload
SRCS=	wsfontload.c

.else

NOPROG=yes

.endif

MAN=	wsfontload.8
MANSUBDIR=	alpha hppa i386 amd64 cats zaurus

.include <bsd.prog.mk>
