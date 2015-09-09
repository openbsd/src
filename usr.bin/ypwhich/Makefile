#	$OpenBSD: Makefile,v 1.7 2015/09/09 15:33:18 deraadt Exp $
#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90

PROG=	ypwhich
SRCS=	ypwhich.c yplib_host.c
MAN=	ypwhich.1

.PATH:	${.CURDIR}/../../usr.sbin/ypserv/common
CFLAGS+=-I${.CURDIR}/../../usr.sbin/ypserv/common

LDADD=-lrpcsvc
DPADD=${LIBRPCSVC}

.include <bsd.prog.mk>
