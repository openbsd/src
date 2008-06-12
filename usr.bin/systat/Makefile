#	$OpenBSD: Makefile,v 1.20 2008/06/12 22:26:01 canacar Exp $

PROG=	systat

.PATH:	${.CURDIR}/../../usr.bin/vmstat

CFLAGS+=-DNOKVM
CPPFLAGS+=-I${.CURDIR}/../../usr.bin/vmstat
CPPFLAGS+=-I${.CURDIR}/../../sbin/pfctl
SRCS=	dkstats.c engine.c if.c iostat.c main.c mbufs.c netstat.c \
	pigs.c sensors.c swap.c vmstat.c pftop.c cache.c pf.c

DPADD=	${LIBCURSES} ${LIBM} ${LIBKVM}
LDADD=	-lcurses -lm -lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>

