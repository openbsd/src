#	$OpenBSD: Makefile,v 1.20 2020/06/29 15:34:07 kettenis Exp $

.if ${MACHINE} == "arm64" || ${MACHINE} == "armv7" || \
    ${MACHINE} == "macppc" || ${MACHINE} == "octeon" || \
    ${MACHINE} == "powerpc64" || ${MACHINE} == "sparc64"
PROG=	eeprom

SRCS=	main.c
SRCS+=	ophandlers.c optree.c

.else
NOPROG=yes
.endif

MAN=	eeprom.8
MANSUBDIR=arm64 armv7 macppc octeon powerpc64 sparc64

.include <bsd.prog.mk>
