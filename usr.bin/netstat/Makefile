#	$OpenBSD: Makefile,v 1.8 2000/06/13 22:27:06 angelos Exp $

PROG=	netstat
SRCS=	if.c inet.c ipx.c iso.c main.c mbuf.c mroute.c ns.c route.c \
	tp_astring.c unix.c atalk.c inet6.c mroute6.c
.PATH:	${.CURDIR}/../../sys/netiso
BINGRP=	kmem
BINMODE=2555
LDADD=	-lkvm
DPADD=	${LIBKVM}
INET6?=	yes

.if (${INET6} != "no")
CPPFLAGS+=	-DINET6
.endif

#CPPFLAGS+=	-DKAME_SCOPEID

.include <bsd.prog.mk>
