#	$OpenBSD: Makefile,v 1.28 2018/05/30 13:43:50 krw Exp $

PROG=	systat

.PATH:	${.CURDIR}/../../usr.bin/vmstat

CFLAGS+=-DNOKVM
CPPFLAGS+=-I${.CURDIR}/../../usr.bin/vmstat
CPPFLAGS+=-I${.CURDIR}/../../sbin/pfctl
SRCS=	dkstats.c engine.c if.c inetname.c iostat.c main.c mbufs.c netstat.c \
	nfs.c pigs.c sensors.c swap.c vmstat.c pftop.c cache.c pf.c \
	pool.c malloc.c cpu.c uvm.c

DPADD=	${LIBCURSES} ${LIBM} ${LIBKVM}
LDADD=	-lcurses -lm -lkvm

.include <bsd.prog.mk>
