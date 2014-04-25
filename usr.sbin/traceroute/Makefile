#	$OpenBSD: Makefile,v 1.7 2014/04/25 12:10:45 florian Exp $

PROG=	traceroute

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual

MAN=	traceroute.8
MLINKS=	traceroute.8 traceroute6.8
BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
