#	$OpenBSD: Makefile,v 1.3 1999/09/26 18:05:32 smurph Exp $

PROG=	installboot
MAN=	installboot.8
MANSUBDIR=/mvme68k

BINDIR=/usr/mdec
DPADD=${LIBUTIL}
LDADD=-lutil

# Need this to work in the miniroot

LDSTATIC=-static

.include <bsd.prog.mk>
