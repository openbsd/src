#	$OpenBSD: Makefile,v 1.5 2004/01/29 08:50:09 mickey Exp $	

.if ${MACHINE} == "i386" || ${MACHINE} == "amd64" ||\
    ${MACHINE} == "alpha" || ${MACHINE} == "pegasos"

PROG=	wsmoused
SRCS=	wsmoused.c mouse_protocols.c

.else

NOPROG=yes

.endif

MAN=	wsmoused.8 
MANSUBDIR=	i386 amd64 alpha pegasos

.include <bsd.prog.mk>

