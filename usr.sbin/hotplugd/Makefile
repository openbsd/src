# $OpenBSD: Makefile,v 1.3 2005/01/18 16:31:24 drahn Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "amd64" || \
    ${MACHINE} == "i386" || ${MACHINE} == "macppc" || \
    ${MACHINE} == "sparc64" || ${MACHINE} == "zaurus"

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
MANSUBDIR=alpha amd64 i386 macppc sparc64 zaurus

.include <bsd.prog.mk>
