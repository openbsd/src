# $OpenBSD: Makefile,v 1.6 2010/07/20 02:06:39 deraadt Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "macppc" || ${MACHINE} == "amd64"

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
MANSUBDIR=i386 macppc amd64

.include <bsd.prog.mk>
