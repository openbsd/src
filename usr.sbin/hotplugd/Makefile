# $OpenBSD: Makefile,v 1.12 2010/01/30 20:48:53 miod Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "amd64" || \
    ${MACHINE} == "armish" || ${MACHINE} == "hppa" || \
    ${MACHINE} == "hppa64" || ${MACHINE} == "i386" || \
    ${MACHINE} == "landisk" || ${MACHINE} == "loongson" || \
    ${MACHINE} == "macppc" || ${MACHINE} == "palm" || \
    ${MACHINE} == "sgi" || ${MACHINE} == "socppc" || \
    ${MACHINE} == "sparc64" || ${MACHINE} == "zaurus"

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

.include <bsd.prog.mk>
