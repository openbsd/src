#	$OpenBSD: Makefile,v 1.22 2006/06/06 00:01:50 drahn Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "amd64" || \
   ${MACHINE} == "cats" || ${MACHINE} == "hp300" || \
   ${MACHINE} == "hppa" || ${MACHINE} == "i386" || \
   ${MACHINE} == "luna88k" || ${MACHINE} == "mac68k" || \
   ${MACHINE} == "macpppc" || ${MACHINE} == "sgi" || \
   ${MACHINE} == "sparc" || ${MACHINE} == "sparc64" || \
   ${MACHINE} == "zaurus"

PROG=	wsconscfg
SRCS=	wsconscfg.c

.else

NOPROG=yes

.endif

MAN=	wsconscfg.8
MANSUBDIR=	alpha amd64 cats hp300 hppa i386 luna88k mac68k macppc \
		sgi sparc sparc64 zaurus

.include <bsd.prog.mk>
