#	$OpenBSD: Makefile,v 1.15 2005/04/12 14:11:49 reyk Exp $

PROG=	netstat
SRCS=	if.c inet.c inet6.c ipx.c main.c mbuf.c mroute.c ns.c route.c \
	unix.c atalk.c mroute6.c net80211.c
BINGRP=	kmem
BINMODE=2555
LDADD=	-lkvm
DPADD=	${LIBKVM}
INET6?=	yes

.if (${INET6} != "no")
CPPFLAGS+=	-DINET6
.endif

.include <bsd.prog.mk>
