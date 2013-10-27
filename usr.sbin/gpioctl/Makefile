# $OpenBSD: Makefile,v 1.7 2013/10/27 03:06:25 aalm Exp $

.if	(${MACHINE} == "i386" || ${MACHINE} == "macppc" || \
	 ${MACHINE} == "amd64" || ${MACHINE} == "armv7")

PROG=	gpioctl
SRCS=	gpioctl.c

CFLAGS+=-Wall
CFLAGS+=-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=-Wmissing-declarations
CFLAGS+=-Wshadow -Wpointer-arith -Wcast-qual

.else

NOPROG=	yes

.endif

MAN=	gpioctl.8
MANSUBDIR=i386 macppc amd64 armv7

.include <bsd.prog.mk>
