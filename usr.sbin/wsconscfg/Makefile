#	$OpenBSD: Makefile,v 1.16 2004/08/07 01:05:24 deraadt Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "amd64" || \
    ${MACHINE} == "cats" || ${MACHINE} == "hppa" || \
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
