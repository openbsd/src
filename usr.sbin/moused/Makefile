#	$OpenBSD: Makefile,v 1.1 2000/09/01 05:59:12 aaron Exp $

.if ${MACHINE} == "i386"

PROG=	moused
SRCS=	moused.c

.else

NOPROG=yes

.endif

MAN=	moused.8 
MANSUBDIR=	i386

.include <bsd.prog.mk>

