#	$OpenBSD: Makefile,v 1.5 1999/06/23 12:56:18 art Exp $

PROG=	fstat
DPADD=	${LIBKVM}
SRCS=	fstat.c isofs.c
LDADD=	-lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>

.if (${UVM} == "yes")
CFLAGS+=-DUVM
.endif
