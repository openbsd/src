#	$OpenBSD: Makefile,v 1.11 2010/02/28 08:36:36 otto Exp $

.if (${MACHINE} == "amd64") || (${MACHINE} == "i386") || \
    (${MACHINE} == "loongson") || (${MACHINE} == "macppc") || \
    (${MACHINE} == "sparc") || (${MACHINE} == "sparc64") || \
    (${MACHINE} == "zaurus")

SRCS=	apmd.c apmsubr.c

PROG=	apmd
.else
NOPROG=yes
.endif

MAN=	apmd.8
MANSUBDIR=amd64 i386 loongson macppc sparc sparc64 zaurus

.include <bsd.prog.mk>
