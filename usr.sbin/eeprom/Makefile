#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
#	$NetBSD: Makefile,v 1.1 1995/07/13 18:07:19 thorpej Exp $

PROG=	eeprom
MAN=	eeprom.8

BINGRP=	kmem
BINMODE=2555

SRCS=	eehandlers.c getdate.c main.c

.if ${MACHINE} == "sparc"
SRCS+=	ophandlers.c
DPADD=	${LIBKVM}
LDADD=	-lkvm
.endif

CLEANFILES+=getdate.c y.tab.h

.include <bsd.prog.mk>
