#	$OpenBSD: Makefile,v 1.12 2016/09/01 10:14:42 tedu Exp $

.if (${MACHINE} == "amd64") || (${MACHINE} == "i386") || \
    (${MACHINE} == "loongson") || (${MACHINE} == "macppc") || \
    (${MACHINE} == "sparc64") || \
    (${MACHINE} == "zaurus")

SRCS=	apmd.c apmsubr.c

PROG=	apmd
.else
NOPROG=yes
.endif

MAN=	apmd.8
MANSUBDIR=amd64 i386 loongson macppc sparc64 zaurus

.include <bsd.prog.mk>
