#	$OpenBSD: Makefile,v 1.6 2000/09/03 18:41:11 espie Exp $

PROG=	ps
SRCS=	fmt.c keyword.c nlist.c print.c ps.c
DPADD=	${LIBM} ${LIBKVM}
LDADD=	-lm -lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
.if (${UVM:L} == "yes")
CFLAGS+=-DUVM
.endif

