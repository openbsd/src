#	$OpenBSD: Makefile,v 1.37 2016/03/31 01:28:01 jsg Exp $

.if ${MACHINE} != "hppa64" && ${MACHINE} != "octeon"

PROG=	wsconscfg
SRCS=	wsconscfg.c

.else

NOPROG=yes

.endif

MAN=	wsconscfg.8

.include <bsd.prog.mk>
