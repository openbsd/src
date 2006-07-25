# $OpenBSD: Makefile,v 1.3 2006/07/25 09:44:49 henning Exp $

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
MANSUBDIR=i386

.include <bsd.prog.mk>
