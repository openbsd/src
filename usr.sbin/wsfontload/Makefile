#	$OpenBSD: Makefile,v 1.7 2004/01/29 02:55:48 deraadt Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "amd64" || \
    ${MACHINE} == "alpha" || ${MACHINE} == "hppa" || \
    ${MACHINE} == "cats"

PROG=	wsfontload
SRCS=	wsfontload.c

.else

NOPROG=yes

.endif

MAN=	wsfontload.8
MANSUBDIR=	alpha hppa i386 amd64 cats

.include <bsd.prog.mk>
