#	$OpenBSD: Makefile,v 1.10 2001/04/10 20:52:10 naddy Exp $

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

.if (${MACHINE_ARCH} == "sparc")
CFLAGS+= -O0
.endif

.include <bsd.prog.mk>
