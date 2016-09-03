#	$OpenBSD: Makefile,v 1.10 2016/09/03 22:00:06 benno Exp $

PROG=	traceroute

SRCS=	traceroute.c worker.c

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual

MAN=	traceroute.8

LINKS=	${BINDIR}/traceroute ${BINDIR}/traceroute6
BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
