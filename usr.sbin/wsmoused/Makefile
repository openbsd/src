#	$OpenBSD: Makefile,v 1.3 2003/10/31 04:12:20 drahn Exp $	

.if ${MACHINE} == "i386" || ${MACHINE} == "alpha" || ${MACHINE} == "pegasos"

PROG=	wsmoused
SRCS=	wsmoused.c mouse_protocols.c

.else

NOPROG=yes

.endif

MAN=	wsmoused.8 
MANSUBDIR=	i386 alpha

.include <bsd.prog.mk>

