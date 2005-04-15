#	$OpenBSD: Makefile,v 1.1 2005/04/15 15:46:21 deraadt Exp $

.PATH:	${.CURDIR}/../cvs

PROG=	rcs
MAN=	rcs.1

SRCS=	rcsprog.c buf.c log.c rcs.c rcsnum.c strtab.c
CFLAGS+=-I${.CURDIR}/../cvs

BINDIR=	/usr/bin

# Don't build the links until we actually support those commands
#LINKS=	${BINDIR}/ci ${BINDIR}/co ${BINDIR}/rcsclean ${BINDIR}/rcsdiff

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare

.include <bsd.prog.mk>
