#	$OpenBSD: Makefile,v 1.7 2005/01/19 02:02:34 uwe Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "macppc") || \
    (${MACHINE} == "zaurus")
SRCS=	apmd.c apmsubr.c

PROG=	apmd
.else
NOPROG=yes
.endif

MAN=	apmd.8
MANSUBDIR=i386 macppc zaurus

.include <bsd.prog.mk>
