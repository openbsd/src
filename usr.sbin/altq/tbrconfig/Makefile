#	$OpenBSD: Makefile,v 1.2 2002/06/05 11:01:55 deraadt Exp $
#	$NetBSD: Makefile,v 1.2 2001/04/05 21:48:05 thorpej Exp $

BINDIR=/sbin
LDSTATIC=-static

PROG=	tbrconfig
MAN=	tbrconfig.8

CPPFLAGS+= -DALTQ -I${.CURDIR}/../libaltq

.include <bsd.prog.mk>
