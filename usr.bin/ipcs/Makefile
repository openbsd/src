#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:49:19 deraadt Exp $

PROG=	ipcs
BINGRP=	kmem
BINMODE= 2555
DPADD+=	${LIBKVM}
LDADD+=	-lkvm

.include <bsd.prog.mk>
