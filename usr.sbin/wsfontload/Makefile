#	$OpenBSD: Makefile,v 1.3 2000/07/20 00:34:29 espie Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "hppa"

PROG=	wsfontload
SRCS=	wsfontload.c

.else

NOPROG=yes

.endif

MAN=	wsfontload.8
MANSUBDIR=	alpha hppa

.include <bsd.prog.mk>
