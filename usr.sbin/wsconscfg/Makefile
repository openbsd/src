#	$OpenBSD: Makefile,v 1.5 2000/09/07 02:12:32 rahnds Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "hppa" || ${MACHINE} == "powerpc"

PROG=	wsconscfg
SRCS=	wsconscfg.c

.else

NOPROG=yes

.endif

MAN=	wsconscfg.8
MANSUBDIR=	alpha hppa powerpc

.include <bsd.prog.mk>
