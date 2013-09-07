#	$OpenBSD: Makefile,v 1.25 2013/09/07 11:43:49 reyk Exp $

PROG=	systat

.PATH:	${.CURDIR}/../../usr.bin/vmstat

CFLAGS+=-DNOKVM
CPPFLAGS+=-I${.CURDIR}/../../usr.bin/vmstat
CPPFLAGS+=-I${.CURDIR}/../../sbin/pfctl
SRCS=	dkstats.c engine.c if.c inetname.c iostat.c main.c mbufs.c netstat.c \
	nfs.c pigs.c sensors.c swap.c vmstat.c pftop.c cache.c pf.c \
	pool.c malloc.c cpu.c

DPADD=	${LIBCURSES} ${LIBM} ${LIBKVM}
LDADD=	-lcurses -lm -lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>

