#	$OpenBSD: Makefile,v 1.29 2024/11/08 08:45:47 matthieu Exp $

PROG=	systat

.PATH:	${.CURDIR}/../../usr.bin/vmstat

CFLAGS+=-DNOKVM
CPPFLAGS+=-I${.CURDIR}/../../usr.bin/vmstat
CPPFLAGS+=-I${.CURDIR}/../../sbin/pfctl
SRCS=	dkstats.c engine.c if.c inetname.c iostat.c main.c mbufs.c netstat.c \
	nfs.c pigs.c sensors.c swap.c vmstat.c pftop.c cache.c pf.c \
	pool.c malloc.c cpu.c uvm.c

DPADD=	${LIBCURSES} ${LIBM} ${LIBKVM}
LDADD=	-lcurses -lm -lkvm -lutil

.include <bsd.prog.mk>
