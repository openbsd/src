#	$OpenBSD: Makefile,v 1.7 2014/07/11 15:29:46 florian Exp $

PROG=	ping

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual

MAN=	ping.8
BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
