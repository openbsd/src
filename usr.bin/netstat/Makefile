#	$OpenBSD: Makefile,v 1.13 2005/02/10 05:28:58 itojun Exp $

PROG=	netstat
SRCS=	if.c inet.c inet6.c ipx.c main.c mbuf.c mroute.c ns.c route.c \
	unix.c atalk.c mroute6.c
BINGRP=	kmem
BINMODE=2555
LDADD=	-lkvm
DPADD=	${LIBKVM}
INET6?=	yes

.if (${INET6} != "no")
CPPFLAGS+=	-DINET6
.endif

CPPFLAGS+=	-DDCCP

.include <bsd.prog.mk>
