#	$OpenBSD: Makefile,v 1.1 2004/02/16 08:54:34 tedu Exp $

PROG=		procmap
SRCS=		main.c 
SRCS=		procmap.c
BINGRP=		kmem
BINMODE=	2555
LDADD=		-lkvm
DPADD=		${LIBKVM}

.include <bsd.prog.mk>
