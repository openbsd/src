#	$OpenBSD: Makefile,v 1.7 2000/04/14 02:40:01 itojun Exp $

PROG=	ifconfig
MAN=	ifconfig.8

CPPFLAGS+=-DINET6
CPPFLAGS+=-DHAVE_IFADDRS_H

# kame scopeid hack
CPPFLAGS+=-DKAME_SCOPEID

.include <bsd.prog.mk>
