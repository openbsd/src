#	$OpenBSD: Makefile,v 1.24 2015/02/12 01:49:02 claudio Exp $

PROG=	netstat
SRCS=	if.c inet.c inet6.c main.c mbuf.c mroute.c route.c \
	unix.c mroute6.c net80211.c show.c
LDADD=	-lkvm -lutil
DPADD=	${LIBKVM} ${LIBUTIL}
CFLAGS+=	-Wall

.include <bsd.prog.mk>
