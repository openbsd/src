#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:48:01 deraadt Exp $

PROG=	pstat
MAN=	pstat.8
BINGRP=	kmem
BINMODE=2555
DPADD=	${LIBKVM}
LDADD=	-lkvm
.if (${MACHINE_ARCH} == "m68k")
CFLAGS+=-D${MACHINE}
.endif

.include <bsd.prog.mk>
