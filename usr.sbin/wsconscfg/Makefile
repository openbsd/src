#	$OpenBSD: Makefile,v 1.9 2002/02/06 15:37:40 jason Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "alpha" || ${MACHINE} == "hppa" || \
	${MACHINE} == "macppc" || ${MACHINE} == "sparc64"

PROG=	wsconscfg
SRCS=	wsconscfg.c

.else

NOPROG=yes

.endif

MAN=	wsconscfg.8
MANSUBDIR=	alpha hppa i386 macppc sparc64

.include <bsd.prog.mk>
