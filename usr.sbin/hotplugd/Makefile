# $OpenBSD: Makefile,v 1.2 2004/05/30 09:22:06 grange Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "amd64" || \
    ${MACHINE} == "i386" || ${MACHINE} == "macppc" || \
    ${MACHINE} == "sparc64"

PROG=	hotplugd
SRCS=	hotplugd.c

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CLFAGS+= -Wmissing-declarations -Wredundant-decls
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual

.else

NOPROG=	yes

.endif

MAN=	hotplugd.8
MANSUBDIR=alpha amd64 i386 macppc sparc64

.include <bsd.prog.mk>
