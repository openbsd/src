#	$OpenBSD: Makefile,v 1.3 2000/07/20 00:34:29 espie Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "hppa"

PROG=	wsconscfg
SRCS=	wsconscfg.c

.else

NOPROG=yes

.endif

MAN=	wsconscfg.8
MANSUBDIR=	alpha hppa

.include <bsd.prog.mk>
