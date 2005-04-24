#	$OpenBSD: Makefile,v 1.1 2005/04/24 18:46:47 uwe Exp $

.if ${MACHINE} == "zaurus"
SRCS=	ztsscale.c

PROG=	ztsscale
.else
NOPROG=yes
.endif

#MAN=	ztsscale.8
#MANSUBDIR=zaurus
NOMAN=yes

.include <bsd.prog.mk>
