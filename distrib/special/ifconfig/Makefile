#	$OpenBSD: Makefile,v 1.1 2004/12/25 11:22:08 deraadt Exp $

PROG=	ifconfig
MAN=	
COPTS+=-Os -DSMALL
.PATH:  ${.CURDIR}/../../../sbin/ifconfig

CPPFLAGS+=-DINET6

# kame scopeid hack
CPPFLAGS+=-DKAME_SCOPEID

.include <bsd.prog.mk>
