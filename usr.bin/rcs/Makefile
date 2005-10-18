#	$OpenBSD: Makefile,v 1.18 2005/10/18 16:20:31 xsa Exp $

.PATH:	${.CURDIR}/../cvs

PROG=	rcs
MAN=	ci.1 co.1 ident.1 rcs.1 rcsclean.1 rcsdiff.1 rcsmerge.1 rlog.1

SRCS=	ci.c co.c ident.c rcsclean.c rcsdiff.c rcsmerge.c rcsprog.c rlog.c \
	buf.c date.y diff.c log.c rcs.c rcsnum.c strtab.c
CPPFLAGS+=-I${.CURDIR}/../cvs -DRCSPROG

LINKS=	${BINDIR}/rcs ${BINDIR}/ci ${BINDIR}/rcs ${BINDIR}/co \
	${BINDIR}/rcs ${BINDIR}/rcsclean ${BINDIR}/rcs ${BINDIR}/rcsdiff
#MLINKS=	rcs.1 ci.1 rcs.1 co.1 rcs.1 rcsclean.1 rcs.1 rcsdiff.1

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare

.include <bsd.prog.mk>
