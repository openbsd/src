#	$OpenBSD: Makefile,v 1.19 2007/12/14 18:35:46 deraadt Exp $

PROG=	netstat
SRCS=	if.c inet.c inet6.c main.c mbuf.c mroute.c route.c \
	unix.c atalk.c mroute6.c net80211.c show.c
BINGRP=	kmem
BINMODE=2555
LDADD=	-lkvm
DPADD=	${LIBKVM}

.include <bsd.prog.mk>
