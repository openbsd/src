#	$OpenBSD: Makefile,v 1.5 2001/09/01 16:21:16 drahn Exp $
#	From: Makefile,v 1.1 1999/05/05 07:37:06 wpaul Exp $

.if (${MACHINE} == "i386") || (${MACHINE_ARCH} == "powerpc") || \
    (${MACHINE} == "alpha")
PROG=	wicontrol
SRCS=	wicontrol.c
.else
NOPROG=
.endif

CFLAGS+= -Wall

MAN=	wicontrol.8

.include <bsd.prog.mk>
