#	$OpenBSD: Makefile,v 1.5 1997/09/21 11:51:02 deraadt Exp $

PROG=	systat

.PATH:	${.CURDIR}/../../usr.bin/vmstat

CFLAGS+=-I${.CURDIR}/../../usr.bin/vmstat
SRCS=	cmds.c cmdtab.c disks.c dkstats.c fetch.c iostat.c keyboard.c \
	main.c mbufs.c netcmds.c netstat.c pigs.c swap.c vmstat.c
DPADD=	${LIBCURSES} ${LIBTERMLIB} ${LIBM} ${LIBKVM}
LDADD=	-lcurses -ltermlib -lm -lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
