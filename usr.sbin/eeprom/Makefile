#	$OpenBSD: Makefile,v 1.2 1997/09/21 11:43:38 deraadt Exp $

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
