#	$OpenBSD: Makefile,v 1.6 2004/05/03 15:18:21 drahn Exp $	

.if ${MACHINE} == "i386" || ${MACHINE} == "amd64" ||\
    ${MACHINE} == "alpha"

PROG=	wsmoused
SRCS=	wsmoused.c mouse_protocols.c

.else

NOPROG=yes

.endif

MAN=	wsmoused.8 
MANSUBDIR=	i386 amd64 alpha

.include <bsd.prog.mk>

