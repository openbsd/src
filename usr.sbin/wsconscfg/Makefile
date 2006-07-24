#	$OpenBSD: Makefile,v 1.24 2006/07/24 22:25:01 miod Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "amd64" || \
   ${MACHINE} == "cats" || ${MACHINE} == "hp300" || \
   ${MACHINE} == "hppa" || ${MACHINE} == "i386" || \
   ${MACHINE} == "luna88k" || ${MACHINE} == "mac68k" || \
   ${MACHINE} == "macppc" || ${MACHINE} == "sgi" || \
   ${MACHINE} == "sparc" || ${MACHINE} == "sparc64" || \
   ${MACHINE} == "vax" || ${MACHINE} == "zaurus"

PROG=	wsconscfg
SRCS=	wsconscfg.c

.else

NOPROG=yes

.endif

MAN=	wsconscfg.8
MANSUBDIR=	alpha amd64 cats hp300 hppa i386 luna88k mac68k macppc \
		sgi sparc sparc64 vax zaurus

.include <bsd.prog.mk>
