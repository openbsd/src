#	$OpenBSD: Makefile,v 1.3 1996/05/22 11:35:41 deraadt Exp $
#	$NetBSD: Makefile,v 1.15 1996/05/10 23:19:25 thorpej Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	vmstat

SRCS=	dkstats.c vmstat.c
MAN=	vmstat.8
DPADD=	${LIBKVM}
LDADD=	-lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
