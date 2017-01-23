# $OpenBSD: Makefile,v 1.8 2017/01/23 03:12:25 jsg Exp $

.if ${MACHINE} == "amd64" || ${MACHINE} == "arm64" || \
    ${MACHINE} == "armv7" || ${MACHINE} == "i386" || \
    ${MACHINE} == "macppc"

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
