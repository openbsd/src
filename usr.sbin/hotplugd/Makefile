# $OpenBSD: Makefile,v 1.19 2016/09/03 13:37:46 guenther Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "amd64" || \
    ${MACHINE} == "armv7" || ${MACHINE} == "hppa" || \
    ${MACHINE} == "i386" || ${MACHINE} == "landisk" || \
    ${MACHINE} == "loongson" || ${MACHINE} == "macppc" || \
    ${MACHINE} == "sgi" || ${MACHINE} == "socppc" || \
    ${MACHINE} == "sparc64"

PROG=	hotplugd
SRCS=	hotplugd.c

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual

.else

NOPROG=	yes

.endif

MAN=	hotplugd.8

.include <bsd.prog.mk>
