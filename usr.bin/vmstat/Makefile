#	$OpenBSD: Makefile,v 1.6 2000/09/03 18:41:20 espie Exp $

PROG=	vmstat

SRCS=	dkstats.c vmstat.c
MAN=	vmstat.8
DPADD=	${LIBKVM}
LDADD=	-lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>

.if (${UVM:L} == "yes")
CFLAGS+=-DUVM
.endif
