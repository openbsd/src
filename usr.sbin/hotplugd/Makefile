# $OpenBSD: Makefile,v 1.11 2009/09/09 12:10:35 marex Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "amd64" || \
    ${MACHINE} == "armish" || ${MACHINE} == "i386" || \
    ${MACHINE} == "hppa" || ${MACHINE} == "landisk" || \
    ${MACHINE} == "macppc" || ${MACHINE} == "sgi" || \
    ${MACHINE} == "socppc" || ${MACHINE} == "sparc64" || \
    ${MACHINE} == "zaurus" || ${MACHINE} == "palm"

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
