#	$NetBSD: Makefile,v 1.7 1995/12/10 10:08:37 mycroft Exp $
#	from: Id: Makefile,v 1.5 1993/06/24 05:11:16 deering Exp

PROG=	mrouted
SRCS=	callout.c cfparse.c config.c igmp.c inet.c kern.c main.c prune.c \
	route.c vif.c
MAN=	mrouted.8

CLEANFILES+= cfparse.c y.tab.h

.include <bsd.prog.mk>
