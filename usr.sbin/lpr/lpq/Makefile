#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$OpenBSD: Makefile,v 1.2 1997/01/17 16:12:44 millert Exp $

PROG=	lpq
CFLAGS+=-I${.CURDIR}/../common_source
SRCS=	lpq.c displayq.c common.c
BINOWN=	root
BINGRP=	daemon
BINMODE=6555
BINDIR=	/usr/bin
.PATH:	${.CURDIR}/../common_source

.include <bsd.prog.mk>
