#	$OpenBSD: Makefile,v 1.4 1998/06/25 06:21:33 deraadt Exp $

PROG=	fstat
DPADD=	${LIBKVM}
SRCS=	fstat.c isofs.c
LDADD=	-lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
