#	$OpenBSD: Makefile,v 1.16 2004/11/16 09:52:33 markus Exp $

PROG=	systat

.PATH:	${.CURDIR}/../../usr.bin/vmstat

CPPFLAGS+=-I${.CURDIR}/../../usr.bin/vmstat
SRCS=	cmds.c cmdtab.c disks.c dkstats.c fetch.c if.c iostat.c keyboard.c \
	main.c mbufs.c netcmds.c netstat.c pigs.c swap.c vmstat.c
DPADD=	${LIBCURSES} ${LIBM} ${LIBKVM}
LDADD=	-lcurses -lm -lkvm
BINGRP=	kmem
BINMODE=2555

.if (${MACHINE_ARCH} == "vax")
cmds.o:
	${CC} ${CFLAGS} ${CPPFLAGS} -O0 -c $<
.endif

.include <bsd.prog.mk>

