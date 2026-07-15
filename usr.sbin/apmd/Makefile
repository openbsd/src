#	$OpenBSD: Makefile,v 1.17 2026/07/15 19:00:24 miod Exp $

.if ((${MACHINE} == "arm64") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "i386") || \
    (${MACHINE} == "macppc") || (${MACHINE} == "sparc64")) || \
    (${MACHINE} == "powerpc64")

SRCS=	apmd.c apmsubr.c

PROG=	apmd
.else
NOPROG=yes
.endif

MAN=	apmd.8

.include <bsd.prog.mk>
