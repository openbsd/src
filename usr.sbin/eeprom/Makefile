#	$OpenBSD: Makefile,v 1.19 2017/12/30 10:10:34 guenther Exp $

.if ${MACHINE} == "arm64" || ${MACHINE} == "armv7" || \
    ${MACHINE} == "macppc" || ${MACHINE} == "octeon" || \
    ${MACHINE} == "sparc64"
PROG=	eeprom

SRCS=	main.c
SRCS+=	ophandlers.c optree.c

.else
NOPROG=yes
.endif

MAN=	eeprom.8
MANSUBDIR=arm64 armv7 macppc octeon sparc64

.include <bsd.prog.mk>
