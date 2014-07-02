#	$OpenBSD: Makefile,v 1.9 2014/07/02 00:12:34 dlg Exp $

PROG=	vmstat

CPPFLAGS+= -D_LIBKVM
SRCS=	dkstats.c vmstat.c
MAN=	vmstat.8
DPADD=	${LIBKVM}
LDADD=	-lkvm

.include <bsd.prog.mk>

