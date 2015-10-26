#	$OpenBSD: Makefile,v 1.7 2015/10/26 10:34:19 tedu Exp $

PROG=	mrouted
SRCS=	callout.c cfparse.c config.c igmp.c inet.c kern.c main.c prune.c \
	route.c vif.c
MAN=	mrouted.8

CFLAGS+=-I${.CURDIR}

CLEANFILES+= cfparse.c y.tab.h

.include <bsd.prog.mk>
