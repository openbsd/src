# $OpenBSD: Makefile,v 1.7 2006/10/14 20:36:40 deraadt Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "amd64" || \
    ${MACHINE} == "armish" || ${MACHINE} == "i386" || \
    ${MACHINE} == "hppa" || ${MACHINE} == "macppc" || \
    ${MACHINE} == "sparc64" || ${MACHINE} == "zaurus" || \
    ${MACHINE} == "landisk"

PROG=	hotplugd
SRCS=	hotplugd.c

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations -Wredundant-decls
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual

.else

NOPROG=	yes

.endif

MAN=	hotplugd.8
MANSUBDIR=alpha amd64 armish i386 hppa macppc sparc64 zaurus landisk

.include <bsd.prog.mk>
