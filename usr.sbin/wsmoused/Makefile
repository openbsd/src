#	$OpenBSD: Makefile,v 1.2 2002/12/03 19:15:07 miod Exp $	

.if ${MACHINE} == "i386" || ${MACHINE} == "alpha"

PROG=	wsmoused
SRCS=	wsmoused.c mouse_protocols.c

.else

NOPROG=yes

.endif

MAN=	wsmoused.8 
MANSUBDIR=	i386 alpha

.include <bsd.prog.mk>

