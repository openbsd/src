#	$OpenBSD: Makefile,v 1.5 2001/01/30 01:18:38 mickey Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "alpha" || ${MACHINE} == "hppa"

PROG=	wsfontload
SRCS=	wsfontload.c

.else

NOPROG=yes

.endif

MAN=	wsfontload.8
MANSUBDIR=	alpha hppa i386

.include <bsd.prog.mk>
