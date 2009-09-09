#	$OpenBSD: Makefile,v 1.13 2009/09/09 12:10:35 marex Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "amd64" || \
    ${MACHINE} == "alpha" || ${MACHINE} == "hppa" || \
    ${MACHINE} == "zaurus" || ${MACHINE} == "palm"

PROG=	wsfontload
SRCS=	wsfontload.c

.else

NOPROG=yes

.endif

MAN=	wsfontload.8

.include <bsd.prog.mk>
