#	$OpenBSD: Makefile,v 1.6 2000/01/17 16:26:18 itojun Exp $

PROG=	fstat
DPADD=	${LIBKVM}
SRCS=	fstat.c isofs.c
LDADD=	-lkvm
BINGRP=	kmem
BINMODE=2555

CFLAGS+=-DINET6

.include <bsd.prog.mk>

.if (${UVM} == "yes")
CFLAGS+=-DUVM
.endif
