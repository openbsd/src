#	$OpenBSD: Makefile,v 1.26 2005/12/10 20:27:46 joris Exp $

.PATH:	${.CURDIR}/../cvs

PROG=	rcs
MAN=	ci.1 co.1 ident.1 rcs.1 rcsclean.1 rcsdiff.1 rcsmerge.1 rlog.1 \
	rcsintro.7

SRCS=	ci.c co.c ident.c rcsclean.c rcsdiff.c rcsmerge.c rcsprog.c rlog.c \
	buf.c date.y diff.c diff3.c fatal.c log.c rcs.c rcsnum.c util.c \
	xmalloc.c

CPPFLAGS+=-I${.CURDIR}/../cvs -DRCSPROG

LINKS=	${BINDIR}/rcs ${BINDIR}/ci ${BINDIR}/rcs ${BINDIR}/co \
	${BINDIR}/rcs ${BINDIR}/rcsclean ${BINDIR}/rcs ${BINDIR}/rcsdiff \
	${BINDIR}/rcs ${BINDIR}/rcsmerge ${BINDIR}/rcs ${BINDIR}/rlog \
	${BINDIR}/rcs ${BINDIR}/ident

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
YFLAGS=

.include <bsd.prog.mk>
