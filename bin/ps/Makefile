#	$OpenBSD: Makefile,v 1.5 2000/05/28 04:22:31 art Exp $

PROG=	ps
SRCS=	fmt.c keyword.c nlist.c print.c ps.c
DPADD=	${LIBM} ${LIBKVM}
LDADD=	-lm -lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
.if (${UVM} == "yes")
CFLAGS+=-DUVM
.endif

