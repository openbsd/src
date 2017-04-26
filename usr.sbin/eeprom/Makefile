#	$OpenBSD: Makefile,v 1.18 2017/04/26 14:53:11 sthen Exp $

.if ${MACHINE} == "arm64" || ${MACHINE} == "armv7" || \
    ${MACHINE} == "macppc" || ${MACHINE} == "octeon" || \
    ${MACHINE} == "sparc64"
PROG=	eeprom

SRCS=	getdate.c main.c
SRCS+=	ophandlers.c optree.c

CLEANFILES+=getdate.c y.tab.h
.else
NOPROG=yes
.endif

MAN=	eeprom.8
MANSUBDIR=arm64 armv7 macppc octeon sparc64

.include <bsd.prog.mk>
