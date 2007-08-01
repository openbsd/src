#	$OpenBSD: Makefile,v 1.26 2007/08/01 21:35:26 deraadt Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "amd64" || \
   ${MACHINE} == "hp300" || ${MACHINE} == "hppa" || \
   ${MACHINE} == "i386" || ${MACHINE} == "luna88k" || \
   ${MACHINE} == "mac68k" || ${MACHINE} == "macppc" || \
   ${MACHINE} == "sgi" || ${MACHINE} == "sparc" || \
   ${MACHINE} == "sparc64" || ${MACHINE} == "vax" || \
   ${MACHINE} == "zaurus"

PROG=	wsconscfg
SRCS=	wsconscfg.c

.else

NOPROG=yes

.endif

MAN=	wsconscfg.8
MANSUBDIR=	alpha amd64 hp300 hppa i386 luna88k mac68k macppc \
		sgi sparc sparc64 vax zaurus

.include <bsd.prog.mk>
