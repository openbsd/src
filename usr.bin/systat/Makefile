#	$OpenBSD: Makefile,v 1.4 1996/06/02 05:54:36 tholo Exp $
#	$NetBSD: Makefile,v 1.5 1996/05/10 23:16:30 thorpej Exp $

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
