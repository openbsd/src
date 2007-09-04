#	$OpenBSD: Makefile,v 1.10 2007/09/04 23:28:26 fgsch Exp $

.if ${MACHINE} == "sparc" || ${MACHINE} == "sparc64"
PROG=	eeprom

BINGRP=	kmem
BINMODE=2555

SRCS=	eehandlers.c getdate.c main.c

.  if ${MACHINE} == "sparc" || ${MACHINE} == "sparc64"
SRCS+=	ophandlers.c optree.c
.  endif

CLEANFILES+=getdate.c y.tab.h
.else
NOPROG=yes
.endif

MAN=	eeprom.8
MANSUBDIR=sparc sparc64

.include <bsd.prog.mk>
