#	$OpenBSD: Makefile,v 1.2 1996/06/26 05:37:19 deraadt Exp $
#	$NetBSD: Makefile,v 1.11 1995/10/03 21:42:34 thorpej Exp $

PROG=	netstat
SRCS=	if.c inet.c iso.c main.c mbuf.c mroute.c ns.c route.c \
	tp_astring.c unix.c
.PATH:	${.CURDIR}/../../sys/netiso
BINGRP=	kmem
BINMODE=2555
LDADD=	-lkvm
DPADD=	${LIBKVM}

.include <bsd.prog.mk>
