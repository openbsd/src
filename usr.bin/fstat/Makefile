#	$OpenBSD: Makefile,v 1.8 2001/06/27 06:16:49 art Exp $

PROG=	fstat
DPADD=	${LIBKVM}
SRCS=	fstat.c isofs.c
LDADD=	-lkvm
BINGRP=	kmem
BINMODE=2555

CFLAGS+=-DINET6

.include <bsd.prog.mk>

