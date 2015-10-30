#	$OpenBSD: Makefile,v 1.8 2015/10/30 11:00:52 florian Exp $

PROG=	ping

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual

MAN=	ping.8

LDADD=	-lm
DPADD=	${LIBM}

BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
