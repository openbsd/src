#	$OpenBSD: Makefile,v 1.15 2004/05/03 15:18:21 drahn Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "amd64" || \
    ${MACHINE} == "cats" || ${MACHINE} == "hppa" || \
    ${MACHINE} == "i386" || ${MACHINE} == "luna88k" || \
    ${MACHINE} == "macppc" || ${MACHINE} == "sparc" || \
    ${MACHINE} == "sparc64"

PROG=	wsconscfg
SRCS=	wsconscfg.c

.else

NOPROG=yes

.endif

MAN=	wsconscfg.8
MANSUBDIR=	alpha amd64 cats hppa i386 luna88k macppc sparc sparc64

.include <bsd.prog.mk>
