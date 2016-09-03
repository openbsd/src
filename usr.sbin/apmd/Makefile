#	$OpenBSD: Makefile,v 1.13 2016/09/03 13:37:46 guenther Exp $

.if (${MACHINE} == "amd64") || (${MACHINE} == "i386") || \
    (${MACHINE} == "loongson") || (${MACHINE} == "macppc") || \
    (${MACHINE} == "sparc64")

SRCS=	apmd.c apmsubr.c

PROG=	apmd
.else
NOPROG=yes
.endif

MAN=	apmd.8
MANSUBDIR=amd64 i386 loongson macppc sparc64

.include <bsd.prog.mk>
