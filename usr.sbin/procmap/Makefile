#	$OpenBSD: Makefile,v 1.4 2004/02/23 21:09:28 tedu Exp $

PROG=		procmap
SRCS=		procmap.c
BINGRP=		kmem
BINMODE=	2555
CFLAGS+=	-Wall
LDADD=		-lkvm
DPADD=		${LIBKVM}

.include <bsd.prog.mk>
