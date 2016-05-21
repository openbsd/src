#	$OpenBSD: Makefile,v 1.14 2016/05/21 21:25:33 kettenis Exp $

.if ${MACHINE} == "armv7" || ${MACHINE} == "macppc" || \
    ${MACHINE} == "sparc" || ${MACHINE} == "sparc64"
PROG=	eeprom

SRCS=	getdate.c main.c
SRCS+=	ophandlers.c optree.c

.  if ${MACHINE} == "sparc"
SRCS+=	eehandlers.c
.  endif

CLEANFILES+=getdate.c y.tab.h
.else
NOPROG=yes
.endif

MAN=	eeprom.8
MANSUBDIR=armv7 macppc sparc sparc64

.include <bsd.prog.mk>
