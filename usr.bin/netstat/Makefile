#	$OpenBSD: Makefile,v 1.20 2011/07/09 00:45:40 henning Exp $

PROG=	netstat
SRCS=	if.c inet.c inet6.c main.c mbuf.c mroute.c route.c \
	unix.c mroute6.c net80211.c show.c
BINGRP=	kmem
BINMODE=2555
LDADD=	-lkvm
DPADD=	${LIBKVM}

.include <bsd.prog.mk>
