#	$OpenBSD: Makefile,v 1.7 2000/09/03 18:41:17 espie Exp $

PROG=	fstat
DPADD=	${LIBKVM}
SRCS=	fstat.c isofs.c
LDADD=	-lkvm
BINGRP=	kmem
BINMODE=2555

CFLAGS+=-DINET6

.include <bsd.prog.mk>

.if (${UVM:L} == "yes")
CFLAGS+=-DUVM
.endif
