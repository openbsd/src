#	$OpenBSD: Makefile,v 1.2 2009/10/28 07:36:49 deraadt Exp $

PROG=	ifconfig
COPTS+=	-DSMALL
.PATH:  ${.CURDIR}/../../../sbin/ifconfig

CPPFLAGS+=-DINET6

# kame scopeid hack
CPPFLAGS+=-DKAME_SCOPEID

.include <bsd.prog.mk>
