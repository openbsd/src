#	$OpenBSD: Makefile,v 1.6 2001/01/30 00:33:04 aaron Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "alpha" || ${MACHINE} == "hppa" || \
	${MACHINE} == "powerpc"

PROG=	wsconscfg
SRCS=	wsconscfg.c

.else

NOPROG=yes

.endif

MAN=	wsconscfg.8
MANSUBDIR=	alpha hppa powerpc

.include <bsd.prog.mk>
