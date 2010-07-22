#	$OpenBSD: Makefile,v 1.24 2010/07/22 12:33:29 giovanni Exp $

PROG=	systat

.PATH:	${.CURDIR}/../../usr.bin/vmstat

CFLAGS+=-DNOKVM
CPPFLAGS+=-I${.CURDIR}/../../usr.bin/vmstat
CPPFLAGS+=-I${.CURDIR}/../../sbin/pfctl
SRCS=	dkstats.c engine.c if.c inetname.c iostat.c main.c mbufs.c netstat.c \
	nfs.c pigs.c sensors.c swap.c vmstat.c pftop.c cache.c pf.c \
	pool.c malloc.c

DPADD=	${LIBCURSES} ${LIBM} ${LIBKVM}
LDADD=	-lcurses -lm -lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>

