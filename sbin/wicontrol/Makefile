#	$OpenBSD: Makefile,v 1.4 2001/07/02 18:13:23 deraadt Exp $
#	From: Makefile,v 1.1 1999/05/05 07:37:06 wpaul Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "powerpc") || \
    (${MACHINE} == "alpha")
PROG=	wicontrol
SRCS=	wicontrol.c
.else
NOPROG=
.endif

CFLAGS+= -Wall

MAN=	wicontrol.8

.include <bsd.prog.mk>
