#	$OpenBSD: Makefile,v 1.9 1998/02/08 19:24:05 deraadt Exp $

#	dump.h			header file
#	itime.c			reads /etc/dumpdates
#	main.c			driver
#	optr.c			operator interface
#	dumprmt.c		handles remote tape via rmt(8)
#	tape.c			handles the mag tape and opening/closing
#	traverse.c		traverses the file system
#
#	DEBUG			use local directory to find ddate and dumpdates
#	TDEBUG			trace out the process forking

PROG=	dump
LINKS=	${BINDIR}/dump ${BINDIR}/rdump
CFLAGS+=-DRDUMP
SRCS=	itime.c main.c optr.c dumprmt.c tape.c traverse.c
MAN=	dump.8
MLINKS+=dump.8 rdump.8

.include <bsd.prog.mk>
