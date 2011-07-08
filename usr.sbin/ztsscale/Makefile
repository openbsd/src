#	$OpenBSD: Makefile,v 1.4 2011/07/08 22:34:05 martynas Exp $

.if ${MACHINE} == "zaurus"
SRCS=	ztsscale.c

PROG=	ztsscale
.else
NOPROG=yes
.endif

MAN=	ztsscale.8
MANSUBDIR=zaurus

.include <bsd.prog.mk>
