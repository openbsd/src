#	$OpenBSD: Makefile,v 1.5 2001/09/20 20:42:25 jason Exp $

.if ${MACHINE} == "sparc" || ${MACHINE} == "sun3" || ${MACHINE} == "sparc64"
PROG=	eeprom

BINGRP=	kmem
BINMODE=2555

# Get getdate.y from cvs distro
.PATH:  ${.CURDIR}/../../gnu/usr.bin/cvs/lib
CFLAGS+=-DTIME_WITH_SYS_TIME -DHAVE_SYS_TIMEB_H -DSTDC_HEADERS -DHAVE_BCOPY

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
