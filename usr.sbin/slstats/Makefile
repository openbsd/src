#	$OpenBSD: Makefile,v 1.2 1997/09/21 11:44:25 deraadt Exp $

PROG=	slstats
SRCS=	slstats.c
MAN=	slstats.8
DPADD=	${LIBKVM}
LDADD=	-lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
