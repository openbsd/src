#	$OpenBSD: Makefile,v 1.5 1999/12/30 16:19:31 itojun Exp $

PROG=	ifconfig
MAN=	ifconfig.8

CPPFLAGS+=-DINET6

# kame scopeid hack
CPPFLAGS+=-DKAME_SCOPEID

.include <bsd.prog.mk>
