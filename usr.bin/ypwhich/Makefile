#	$OpenBSD: Makefile,v 1.8 2017/07/09 21:23:19 espie Exp $
#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90

PROG=	ypwhich
SRCS=	ypwhich.c yplib_host.c

.PATH:	${.CURDIR}/../../usr.sbin/ypserv/common
CFLAGS+=-I${.CURDIR}/../../usr.sbin/ypserv/common

LDADD=-lrpcsvc
DPADD=${LIBRPCSVC}

.include <bsd.prog.mk>
