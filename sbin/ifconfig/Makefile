#	$OpenBSD: Makefile,v 1.8 2004/04/27 22:14:11 pb Exp $

PROG=	ifconfig
MAN=	ifconfig.8

CPPFLAGS+=-DINET6

# kame scopeid hack
CPPFLAGS+=-DKAME_SCOPEID

.include <bsd.prog.mk>
