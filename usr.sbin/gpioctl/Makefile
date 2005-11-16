# $OpenBSD: Makefile,v 1.2 2005/11/16 03:42:12 deraadt Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "macppc"

PROG=	gpioctl
SRCS=	gpioctl.c

CFLAGS+=-Wall
CFLAGS+=-Wstrict-prototypes -Wmissing-prototypes
CLFAGS+=-Wmissing-declarations -Wredundant-decls
CFLAGS+=-Wshadow -Wpointer-arith -Wcast-qual

.else

NOPROG=	yes

.endif

MAN=	gpioctl.8
MANSUBDIR=i386

.include <bsd.prog.mk>
