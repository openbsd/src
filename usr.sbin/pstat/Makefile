#	$OpenBSD: Makefile,v 1.5 2000/09/03 18:41:22 espie Exp $

PROG=	pstat
MAN=	pstat.8
BINGRP=	kmem
BINMODE=2555
DPADD=	${LIBKVM}
LDADD=	-lkvm

.include <bsd.prog.mk>

.if (${UVM:L} == "yes")
CFLAGS+=-DUVM
.endif

