#	$OpenBSD: Makefile,v 1.3 1996/08/16 09:29:31 mickey Exp $
#	$NetBSD: Makefile,v 1.11 1995/10/03 21:42:34 thorpej Exp $

PROG=	netstat
SRCS=	if.c inet.c ipx.c iso.c main.c mbuf.c mroute.c ns.c route.c \
	tp_astring.c unix.c
.PATH:	${.CURDIR}/../../sys/netiso
BINGRP=	kmem
BINMODE=2555
LDADD=	-lkvm
DPADD=	${LIBKVM}

.include <bsd.prog.mk>
