#	$OpenBSD: Makefile,v 1.8 2005/03/29 16:26:45 miod Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "macppc") || \
    (${MACHINE} == "sparc") || (${MACHINE} == "zaurus")
SRCS=	apmd.c apmsubr.c

PROG=	apmd
.else
NOPROG=yes
.endif

MAN=	apmd.8
MANSUBDIR=i386 macppc sparc zaurus

.include <bsd.prog.mk>
