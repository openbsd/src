#	$OpenBSD: Makefile,v 1.2 2005/04/15 16:15:34 deraadt Exp $

.PATH:	${.CURDIR}/../cvs

PROG=	rcs
MAN=	rcs.1

SRCS=	rcsprog.c buf.c log.c rcs.c rcsnum.c strtab.c
CFLAGS+=-I${.CURDIR}/../cvs

# Don't build the links until we actually support those commands
#LINKS=	${BINDIR}/ci ${BINDIR}/co ${BINDIR}/rcsclean ${BINDIR}/rcsdiff

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare

.include <bsd.prog.mk>
