#	$OpenBSD: Makefile,v 1.4 2000/09/07 01:37:25 rahnds Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "hppa" || ${MACHINE} == "powerpc"

PROG=	wsconscfg
SRCS=	wsconscfg.c

.else

NOPROG=yes

.endif

MAN=	wsconscfg.8
MANSUBDIR=	alpha hppa

.include <bsd.prog.mk>
