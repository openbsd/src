#	$OpenBSD: Makefile,v 1.3 2000/01/03 19:49:44 millert Exp $

PROG=	eeprom
MAN=	eeprom.8

BINGRP=	kmem
BINMODE=2555

# Get getdate.y from cvs distro
.PATH:  ${.CURDIR}/../../gnu/usr.bin/cvs/lib
CFLAGS+=-DTIME_WITH_SYS_TIME -DHAVE_SYS_TIMEB_H -DSTDC_HEADERS -DHAVE_BCOPY

SRCS=	eehandlers.c getdate.c main.c

.if ${MACHINE} == "sparc"
SRCS+=	ophandlers.c
DPADD=	${LIBKVM}
LDADD=	-lkvm
.endif

CLEANFILES+=getdate.c y.tab.h

.include <bsd.prog.mk>
