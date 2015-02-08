#	$OpenBSD: Makefile,v 1.23 2015/02/08 02:02:21 claudio Exp $

PROG=	netstat
SRCS=	if.c inet.c inet6.c main.c mbuf.c mroute.c route.c \
	unix.c mroute6.c net80211.c show.c
BINGRP=	kmem
BINMODE=2555
LDADD=	-lkvm -lutil
DPADD=	${LIBKVM} ${LIBUTIL}
CFLAGS+=	-Wall

.include <bsd.prog.mk>
