#	$OpenBSD: Makefile,v 1.15 2016/09/03 13:37:46 guenther Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "amd64" || \
    ${MACHINE} == "alpha" || ${MACHINE} == "hppa"

PROG=	wsfontload
SRCS=	wsfontload.c

.else

NOPROG=yes

.endif

MAN=	wsfontload.8

.include <bsd.prog.mk>
