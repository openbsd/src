#	$OpenBSD: Makefile,v 1.7 2001/12/04 19:18:31 miod Exp $

.if ${MACHINE} == "sparc" || ${MACHINE} == "sun3" || ${MACHINE} == "sparc64"
PROG=	eeprom

BINGRP=	kmem
BINMODE=2555

SRCS=	eehandlers.c getdate.c main.c

.  if ${MACHINE} == "sparc" || ${MACHINE} == "sparc64"
SRCS+=	ophandlers.c
DPADD=	${LIBKVM}
LDADD=	-lkvm
.  endif

CLEANFILES+=getdate.c y.tab.h
.else
NOPROG=yes
.endif

MAN=	eeprom.8
MANSUBDIR=sparc sparc64 sun3

.include <bsd.prog.mk>
