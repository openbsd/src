#	$OpenBSD: Makefile,v 1.15 2005/10/15 20:06:07 niallo Exp $

.PATH:	${.CURDIR}/../cvs

PROG=	rcs
MAN=	ci.1 co.1 ident.1 rcs.1 rcsclean.1 rcsdiff.1 rlog.1

SRCS=	ci.c co.c ident.c rcsclean.c rcsdiff.c rcsprog.c rlog.c buf.c date.y \
	diff.c log.c rcs.c rcsnum.c strtab.c
CPPFLAGS+=-I${.CURDIR}/../cvs -DRCSPROG

# Don't build the links until we actually support those commands
#LINKS=	${BINDIR}/ci ${BINDIR}/co ${BINDIR}/rcsclean ${BINDIR}/rcsdiff
#MLINKS=	rcs.1 ci.1 rcs.1 co.1 rcs.1 rcsclean.1 rcs.1 rcsdiff.1

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare

.include <bsd.prog.mk>
