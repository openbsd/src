#	$OpenBSD: Makefile,v 1.8 2004/01/28 17:45:22 deraadt Exp $
#	From: Makefile,v 1.1 1999/05/05 07:37:06 wpaul Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "alpha") || (${MACHINE} == "sparc") || \
    (${MACHINE} == "sparc64") || (${MACHINE_ARCH} == "powerpc")
PROG=	wicontrol
SRCS=	wicontrol.c
.else
NOPROG=
.endif

CFLAGS+= -Wall

MAN=	wicontrol.8

.include <bsd.prog.mk>
