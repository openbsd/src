#	$OpenBSD: Makefile,v 1.2 2000/07/02 02:07:10 mickey Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "hppa"

PROG=	wsconscfg
SRCS=	wsconscfg.c

.else

NOPROG=yes

.endif

MAN=	wsconscfg.8

.include <bsd.prog.mk>
