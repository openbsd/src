#	$OpenBSD: Makefile,v 1.6 2001/09/30 19:48:23 millert Exp $

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
MANSUBDIR=sparc sun3

.include <bsd.prog.mk>
