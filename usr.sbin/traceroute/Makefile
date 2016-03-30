#	$OpenBSD: Makefile,v 1.9 2016/03/30 06:38:47 jmc Exp $

PROG=	traceroute

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual

MAN=	traceroute.8

LINKS=	${BINDIR}/traceroute ${BINDIR}/traceroute6
BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
