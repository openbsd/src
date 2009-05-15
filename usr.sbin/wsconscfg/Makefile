#	$OpenBSD: Makefile,v 1.27 2009/05/15 21:45:44 deraadt Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "amd64" || \
   ${MACHINE} == "hp300" || ${MACHINE} == "hppa" || \
   ${MACHINE} == "i386" || ${MACHINE} == "luna88k" || \
   ${MACHINE} == "mac68k" || ${MACHINE} == "macppc" || \
   ${MACHINE} == "sgi" || ${MACHINE} == "sparc" || \
   ${MACHINE} == "sparc64" || ${MACHINE} == "vax" || \
   ${MACHINE} == "zaurus" || ${MACHINE} == "landisk" ||
   ${MACHINE} == "socppc"

PROG=	wsconscfg
SRCS=	wsconscfg.c

.else

NOPROG=yes

.endif

MAN=	wsconscfg.8

.include <bsd.prog.mk>
