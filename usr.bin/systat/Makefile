#	$OpenBSD: Makefile,v 1.12 2000/09/03 18:41:20 espie Exp $

PROG=	systat

.PATH:	${.CURDIR}/../../usr.bin/vmstat

CPPFLAGS+=-I${.CURDIR}/../../usr.bin/vmstat
SRCS=	cmds.c cmdtab.c disks.c dkstats.c fetch.c iostat.c keyboard.c \
	main.c mbufs.c netcmds.c netstat.c pigs.c swap.c vmstat.c
DPADD=	${LIBCURSES} ${LIBM} ${LIBKVM}
LDADD=	-lcurses -lm -lkvm
BINGRP=	kmem
BINMODE=2555

CPPFLAGS+=-DINET6

.include <bsd.prog.mk>

.if (${UVM:L} == "yes")
CFLAGS+=-DUVM
.endif
