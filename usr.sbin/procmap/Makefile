#	$OpenBSD: Makefile,v 1.3 2004/02/23 21:07:32 tedu Exp $

PROG=		procmap
SRCS=		main.c 
SRCS=		procmap.c
BINGRP=		kmem
BINMODE=	2555
CFLAGS+=	-Wall
LDADD=		-lkvm
DPADD=		${LIBKVM}

.include <bsd.prog.mk>
