#	$OpenBSD: Makefile,v 1.7 1998/01/05 00:01:55 deraadt Exp $

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
