#	$OpenBSD: Makefile,v 1.1 2005/03/05 18:50:43 jfb Exp $

.PATH:	${.CURDIR}/..

PROG=	rcs
MAN=	rcs.1

SRCS=	rcsprog.c buf.c log.c rcs.c rcsnum.c

BINDIR=	/usr/bin

# Don't build the links until we actually support those commands
#LINKS=	${BINDIR}/ci ${BINDIR}/co ${BINDIR}/rcsclean ${BINDIR}/rcsdiff

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare

.include <bsd.prog.mk>
