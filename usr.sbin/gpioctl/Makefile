# $OpenBSD: Makefile,v 1.1 2004/06/03 19:13:24 grange Exp $

.if ${MACHINE} == "i386"

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
