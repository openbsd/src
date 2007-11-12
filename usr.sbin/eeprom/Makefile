#	$OpenBSD: Makefile,v 1.11 2007/11/12 20:54:54 kettenis Exp $

.if ${MACHINE} == "macppc" || ${MACHINE} == "sparc" || ${MACHINE} == "sparc64"
PROG=	eeprom

BINGRP=	kmem
BINMODE=2555

SRCS=	getdate.c main.c
SRCS+=	ophandlers.c optree.c

.  if ${MACHINE} == "sparc" || ${MACHINE} == "sparc64"
SRCS+=	eehandlers.c
.  endif

CLEANFILES+=getdate.c y.tab.h
.else
NOPROG=yes
.endif

MAN=	eeprom.8
MANSUBDIR=macppc sparc sparc64

.include <bsd.prog.mk>
