#	$OpenBSD: Makefile,v 1.4 1999/06/23 11:47:29 art Exp $

PROG=	pstat
MAN=	pstat.8
BINGRP=	kmem
BINMODE=2555
DPADD=	${LIBKVM}
LDADD=	-lkvm

.include <bsd.prog.mk>

.if (${UVM} == "yes")
CFLAGS+=-DUVM
.endif

