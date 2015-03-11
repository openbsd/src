#	$OpenBSD: Makefile,v 1.13 2015/03/11 18:12:27 deraadt Exp $

.if ${MACHINE} == "macppc" || ${MACHINE} == "sparc" || ${MACHINE} == "sparc64"
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
MANSUBDIR=macppc sparc sparc64

.include <bsd.prog.mk>
