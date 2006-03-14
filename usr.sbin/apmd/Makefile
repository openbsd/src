#	$OpenBSD: Makefile,v 1.9 2006/03/14 22:30:53 sturm Exp $

.if (${MACHINE} == "amd64") || (${MACHINE} == "i386") || \
    (${MACHINE} == "macppc") || (${MACHINE} == "sparc") || \
    (${MACHINE} == "zaurus")
SRCS=	apmd.c apmsubr.c

PROG=	apmd
.else
NOPROG=yes
.endif

MAN=	apmd.8
MANSUBDIR=amd64 i386 macppc sparc zaurus

.include <bsd.prog.mk>
