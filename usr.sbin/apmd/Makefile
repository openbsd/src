#	$OpenBSD: Makefile,v 1.5 2000/09/21 14:59:40 mickey Exp $

.if ${MACHINE} == "i386"
SRCS=	apmd.c apmsubr.c

PROG=	apmd
.else
NOPROG=yes
.endif

MAN=	apmd.8
MANSUBDIR=i386

.include <bsd.prog.mk>
