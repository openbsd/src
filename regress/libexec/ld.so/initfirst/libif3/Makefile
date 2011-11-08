#	$OpenBSD: Makefile,v 1.1 2011/11/08 23:51:02 guenther Exp $

LIB=		if3
SRCS=		lib.c
LDADD+=		-Wl,-zinitfirst

regress: all

.include <bsd.lib.mk>
