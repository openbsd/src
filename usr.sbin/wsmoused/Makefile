#	$OpenBSD: Makefile,v 1.4 2004/01/28 17:43:01 deraadt Exp $	

.if ${MACHINE} == "i386" || ${MACHINE} == "amd64" ||\
    ${MACHINE} == "alpha" || ${MACHINE} == "pegasos"

PROG=	wsmoused
SRCS=	wsmoused.c mouse_protocols.c

.else

NOPROG=yes

.endif

MAN=	wsmoused.8 
MANSUBDIR=	i386 amd64 alpha pegosos

.include <bsd.prog.mk>

