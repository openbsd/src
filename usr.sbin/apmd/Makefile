#	$OpenBSD: Makefile,v 1.6 2001/10/03 19:07:58 drahn Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "macppc")
SRCS=	apmd.c apmsubr.c

PROG=	apmd
.else
NOPROG=yes
.endif

MAN=	apmd.8
MANSUBDIR=i386 macppc

.include <bsd.prog.mk>
