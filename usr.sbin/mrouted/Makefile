#	$OpenBSD: Makefile,v 1.5 2004/01/05 02:55:28 espie Exp $

PROG=	mrouted
SRCS=	callout.c cfparse.c config.c igmp.c inet.c kern.c main.c prune.c \
	route.c vif.c
MAN=	mrouted.8

LDADD+=	-lutil
CFLAGS+=-I${.CURDIR}
DPADD+=	${LIBUTIL}

CLEANFILES+= cfparse.c y.tab.h

.include <bsd.prog.mk>
