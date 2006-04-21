#	$OpenBSD: Makefile,v 1.31 2006/04/21 17:17:29 xsa Exp $

.PATH:	${.CURDIR}/../cvs

PROG=	rcs
MAN=	ci.1 co.1 ident.1 rcs.1 rcsclean.1 rcsdiff.1 rcsmerge.1 rlog.1

SRCS=	ci.c co.c ident.c rcsclean.c rcsdiff.c rcsmerge.c rcsprog.c rlog.c \
	rcsutil.c buf.c date.y diff.c diff3.c fatal.c log.c rcs.c rcsnum.c \
	rcstime.c util.c worklist.c xmalloc.c

CPPFLAGS+=-I${.CURDIR}/../cvs -DRCSPROG

LINKS=	${BINDIR}/rcs ${BINDIR}/ci ${BINDIR}/rcs ${BINDIR}/co \
	${BINDIR}/rcs ${BINDIR}/rcsclean ${BINDIR}/rcs ${BINDIR}/rcsdiff \
	${BINDIR}/rcs ${BINDIR}/rcsmerge ${BINDIR}/rcs ${BINDIR}/rlog \
	${BINDIR}/rcs ${BINDIR}/ident

CFLAGS+=-Wall
CFLAGS+=-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=-Wmissing-declarations
CFLAGS+=-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=-Wsign-compare
DEBUG=	-g -ggdb
YFLAGS=

.include <bsd.prog.mk>
