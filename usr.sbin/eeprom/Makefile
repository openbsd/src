#	$OpenBSD: Makefile,v 1.4 2000/05/18 16:46:37 espie Exp $

.if ${MACHINE} == "sparc" || ${MACHINE} == "sun3"
PROG=	eeprom

BINGRP=	kmem
BINMODE=2555

# Get getdate.y from cvs distro
.PATH:  ${.CURDIR}/../../gnu/usr.bin/cvs/lib
CFLAGS+=-DTIME_WITH_SYS_TIME -DHAVE_SYS_TIMEB_H -DSTDC_HEADERS -DHAVE_BCOPY

SRCS=	eehandlers.c getdate.c main.c

.  if ${MACHINE} == "sparc"
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
