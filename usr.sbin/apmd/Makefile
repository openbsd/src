#	$OpenBSD: Makefile,v 1.10 2008/08/11 18:45:13 kettenis Exp $

.if (${MACHINE} == "amd64") || (${MACHINE} == "i386") || \
    (${MACHINE} == "macppc") || (${MACHINE} == "sparc") || \
    (${MACHINE} == "sparc64") || (${MACHINE} == "zaurus")
SRCS=	apmd.c apmsubr.c

PROG=	apmd
.else
NOPROG=yes
.endif

MAN=	apmd.8
MANSUBDIR=amd64 i386 macppc sparc sparc64 zaurus

.include <bsd.prog.mk>
