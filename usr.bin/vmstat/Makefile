#	$NetBSD: Makefile,v 1.14 1995/05/07 22:15:57 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	vmstat
.if (${MACHINE_ARCH} == "m68k")
CFLAGS+=-D${MACHINE}
.endif
CFLAGS+=-I${.CURDIR}/../../sys/arch
MAN=	vmstat.8
BINGRP=	kmem
BINMODE=2555
DPADD=	names.c ${LIBKVM}
LDADD=	-lkvm

.include <bsd.prog.mk>
