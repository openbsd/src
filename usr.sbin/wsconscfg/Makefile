#	$OpenBSD: Makefile,v 1.8 2001/09/01 17:01:49 drahn Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "alpha" || ${MACHINE} == "hppa" || \
	${MACHINE} == "macppc"

PROG=	wsconscfg
SRCS=	wsconscfg.c

.else

NOPROG=yes

.endif

MAN=	wsconscfg.8
MANSUBDIR=	alpha hppa i386 macppc

.include <bsd.prog.mk>
