#	$OpenBSD: Makefile,v 1.4 2002/08/01 03:45:53 itojun Exp $

PROG=	mrouted
SRCS=	callout.c cfparse.c config.c igmp.c inet.c kern.c main.c prune.c \
	route.c vif.c
MAN=	mrouted.8

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

CLEANFILES+= cfparse.c y.tab.h

.include <bsd.prog.mk>
