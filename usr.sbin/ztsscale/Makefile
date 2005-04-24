#	$OpenBSD: Makefile,v 1.2 2005/04/24 22:23:55 deraadt Exp $

.if ${MACHINE} == "zaurus"
SRCS=	ztsscale.c

PROG=	ztsscale
.else
NOPROG=yes
.endif

MAN=	ztsscale.8
MANSUBDIR=zaurus

.include <bsd.prog.mk>
