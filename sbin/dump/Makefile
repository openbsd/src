#	$OpenBSD: Makefile,v 1.3 1996/09/01 15:31:01 deraadt Exp $
#	$NetBSD: Makefile,v 1.16 1995/03/18 14:54:53 cgd Exp $

#	dump.h			header file
#	itime.c			reads /etc/dumpdates
#	main.c			driver
#	optr.c			operator interface
#	dumprmt.c		handles remote tape via rmt(8)
#	tape.c			handles the mag tape and opening/closing
#	traverse.c		traverses the file system
#	unctime.c		undo ctime
#
#	DEBUG			use local directory to find ddate and dumpdates
#	TDEBUG			trace out the process forking

PROG=	dump
LINKS=	${BINDIR}/dump ${BINDIR}/rdump
CFLAGS+=-DRDUMP
SRCS=	itime.c main.c optr.c dumprmt.c tape.c traverse.c unctime.c
BINGRP=	tty
BINMODE=2555
MAN=	dump.8
MLINKS+=dump.8 rdump.8

.include <bsd.prog.mk>
