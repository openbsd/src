#	$OpenBSD: Makefile,v 1.7 2014/07/16 20:07:03 okan Exp $	

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
