#	$OpenBSD: Makefile,v 1.4 2001/01/30 00:33:04 aaron Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "alpha" || ${MACHINE} == "hppa"

PROG=	wsfontload
SRCS=	wsfontload.c

.else

NOPROG=yes

.endif

MAN=	wsfontload.8
MANSUBDIR=	alpha hppa

.include <bsd.prog.mk>
