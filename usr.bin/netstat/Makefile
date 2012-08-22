#	$OpenBSD: Makefile,v 1.21 2012/08/22 00:11:57 tedu Exp $

PROG=	netstat
SRCS=	if.c inet.c inet6.c main.c mbuf.c mroute.c route.c \
	unix.c mroute6.c net80211.c show.c
BINGRP=	kmem
BINMODE=2555
LDADD=	-lkvm -lutil
DPADD=	${LIBKVM} ${LIBUTIL}

.include <bsd.prog.mk>
