#	$OpenBSD: Makefile,v 1.1.1.1 2001/06/27 18:23:36 kjc Exp $
#	$NetBSD: Makefile,v 1.2 2001/04/05 21:48:05 thorpej Exp $

BINDIR=/sbin

PROG=	tbrconfig
MAN=	tbrconfig.8

CPPFLAGS+= -DALTQ -I${.CURDIR}/../libaltq

.include <bsd.prog.mk>
