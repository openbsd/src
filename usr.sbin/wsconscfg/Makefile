#	$OpenBSD: Makefile,v 1.7 2001/01/30 01:18:38 mickey Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "alpha" || ${MACHINE} == "hppa" || \
	${MACHINE} == "powerpc"

PROG=	wsconscfg
SRCS=	wsconscfg.c

.else

NOPROG=yes

.endif

MAN=	wsconscfg.8
MANSUBDIR=	alpha hppa i386 powerpc

.include <bsd.prog.mk>
