#	$OpenBSD: Makefile,v 1.2 1996/03/03 02:51:20 tholo Exp $
#	$NetBSD: Makefile,v 1.14 1995/05/07 22:15:57 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	vmstat
.if (${MACHINE_ARCH} == "m68k")
CFLAGS+=-D${MACHINE}
.endif

CFLAGS+=-I${.CURDIR}/../../sys/arch
SRCS=	dkstats.c vmstat.c
MAN=	vmstat.8
DPADD=	${LIBKVM}
LDADD=	-lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
