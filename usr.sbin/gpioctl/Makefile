# $OpenBSD: Makefile,v 1.5 2009/08/10 14:48:27 jsg Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "macppc" || ${MACHINE} == "amd64"

PROG=	gpioctl
SRCS=	gpioctl.c

CFLAGS+=-Wall
CFLAGS+=-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=-Wmissing-declarations -Wredundant-decls
CFLAGS+=-Wshadow -Wpointer-arith -Wcast-qual

.else

NOPROG=	yes

.endif

MAN=	gpioctl.8
MANSUBDIR=i386 macppc amd64

.include <bsd.prog.mk>
