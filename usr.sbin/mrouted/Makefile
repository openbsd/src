#	$OpenBSD: Makefile,v 1.6 2015/10/26 10:08:15 jung Exp $

PROG=	mrouted
SRCS=	callout.c cfparse.c config.c igmp.c inet.c kern.c main.c prune.c \
	route.c vif.c
MAN=	mrouted.8

CFLAGS+=-I${.CURDIR}
DPADD+=	${LIBUTIL}

CLEANFILES+= cfparse.c y.tab.h

.include <bsd.prog.mk>
