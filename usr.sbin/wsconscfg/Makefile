#	$OpenBSD: Makefile,v 1.19 2005/01/14 22:39:40 miod Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "amd64" || \
    ${MACHINE} == "cats" || ${MACHINE} == "hp300" || \
    ${MACHINE} == "hppa" || ${MACHINE} == "i386" || \
    ${MACHINE} == "luna88k" || ${MACHINE} == "macppc" || \
    ${MACHINE} == "sgi" || ${MACHINE} == "sparc" || \
    ${MACHINE} == "sparc64" || ${MACHINE} == "zaurus"

PROG=	wsconscfg
SRCS=	wsconscfg.c

.else

NOPROG=yes

.endif

MAN=	wsconscfg.8
MANSUBDIR=	alpha amd64 cats hp300 hppa i386 luna88k macppc sgi
MANSUBDIR+=	sparc sparc64 zaurus

.include <bsd.prog.mk>
