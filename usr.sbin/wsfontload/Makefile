#	$OpenBSD: Makefile,v 1.2 2000/07/02 02:07:11 mickey Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "hppa"

PROG=	wsfontload
SRCS=	wsfontload.c

.else

NOPROG=yes

.endif

MAN=	wsfontload.8

.include <bsd.prog.mk>
