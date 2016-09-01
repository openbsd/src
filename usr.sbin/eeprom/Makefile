#	$OpenBSD: Makefile,v 1.16 2016/09/01 10:00:38 tedu Exp $

.if ${MACHINE} == "armv7" || ${MACHINE} == "macppc" || \
    ${MACHINE} == "octeon" || \
    ${MACHINE} == "sparc64"
PROG=	eeprom

SRCS=	getdate.c main.c
SRCS+=	ophandlers.c optree.c

CLEANFILES+=getdate.c y.tab.h
.else
NOPROG=yes
.endif

MAN=	eeprom.8
MANSUBDIR=armv7 macppc octeon sparc64

.include <bsd.prog.mk>
