#	$OpenBSD: Makefile,v 1.8 2005/10/05 23:11:15 niallo Exp $

.PATH:	${.CURDIR}/../cvs

PROG=	rcs
MAN=	rcs.1

SRCS=	ci.c co.c rcsprog.c buf.c diff.c log.c rcs.c rcsnum.c strtab.c
CFLAGS+=-I${.CURDIR}/../cvs

# Don't build the links until we actually support those commands
#LINKS=	${BINDIR}/ci ${BINDIR}/co ${BINDIR}/rcsclean ${BINDIR}/rcsdiff

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -DRCSPROG

.include <bsd.prog.mk>
