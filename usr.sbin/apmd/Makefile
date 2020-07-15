#	$OpenBSD: Makefile,v 1.15 2020/07/15 22:46:52 deraadt Exp $

.if ((${MACHINE} == "arm64") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "i386") || (${MACHINE} == "loongson") || \
    (${MACHINE} == "macppc") || (${MACHINE} == "sparc64")) || \
    (${MACHINE} == "powerpc64")

SRCS=	apmd.c apmsubr.c

PROG=	apmd
.else
NOPROG=yes
.endif

MAN=	apmd.8
MANSUBDIR= arm64 amd64 i386 loongson macppc sparc64

.include <bsd.prog.mk>
