# $OpenBSD: Makefile,v 1.4 2006/11/29 22:15:33 deraadt Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "macppc"

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
MANSUBDIR=i386 macppc

.include <bsd.prog.mk>
