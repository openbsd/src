#	$OpenBSD: Makefile,v 1.2 2004/02/23 21:06:26 tedu Exp $

PROG=		procmap
SRCS=		main.c 
SRCS=		procmap.c
BINGRP=		kmem
BINMODE=	2555
COPTS=		-Wall
LDADD=		-lkvm
DPADD=		${LIBKVM}

.include <bsd.prog.mk>
