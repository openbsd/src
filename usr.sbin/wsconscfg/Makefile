#	$OpenBSD: Makefile,v 1.13 2004/01/29 02:55:48 deraadt Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "amd64" || \
    ${MACHINE} == "alpha" || ${MACHINE} == "hppa" || \
    ${MACHINE} == "macppc" || ${MACHINE} == "sparc" || \
    ${MACHINE} == "sparc64" || ${MACHINE} == "pegasos" || \
    ${MACHINE} == "cats"

PROG=	wsconscfg
SRCS=	wsconscfg.c

.else

NOPROG=yes

.endif

MAN=	wsconscfg.8
MANSUBDIR=	alpha hppa i386 macppc pegasos sparc sparc64 amd64 cats

.include <bsd.prog.mk>
