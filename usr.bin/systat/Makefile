#	$NetBSD: Makefile,v 1.3 1995/03/22 15:26:42 mycroft Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	systat
CFLAGS+=-I/sys -I${.CURDIR}/../vmstat
SRCS=	cmds.c cmdtab.c disks.c fetch.c iostat.c keyboard.c main.c mbufs.c \
	netcmds.c netstat.c pigs.c swap.c vmstat.c
LDADD=	-lcurses -ltermcap -lm -lkvm
DPADD=	${LIBCURSES} ${LIBTERMCAP} ${LIBM} ${LIBKVM}
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
