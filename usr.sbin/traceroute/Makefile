#	$OpenBSD: Makefile,v 1.6 2014/04/23 08:44:50 florian Exp $

PROG=	traceroute

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual

MAN=	traceroute.8
BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
