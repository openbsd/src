#	$OpenBSD: Makefile,v 1.6 2002/06/20 02:52:40 fgsch Exp $
#	From: Makefile,v 1.1 1999/05/05 07:37:06 wpaul Exp $

.if (${MACHINE} == "i386") || (${MACHINE_ARCH} == "powerpc") || \
    (${MACHINE} == "alpha") || (${MACHINE_ARCH} == "sparc64")
PROG=	wicontrol
SRCS=	wicontrol.c
.else
NOPROG=
.endif

CFLAGS+= -Wall

MAN=	wicontrol.8

.include <bsd.prog.mk>
