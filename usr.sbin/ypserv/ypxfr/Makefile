#	$OpenBSD: Makefile,v 1.2 1996/05/30 09:53:34 deraadt Exp $

PROG=	ypxfr
SRCS=	ypxfr.c ypdb.c yplog.c yplib_host.c ypxfr_xdr.c
MAN=	ypxfr.8
.PATH:	${.CURDIR}/../common
CFLAGS+=-I${.CURDIR}/../common

.include <bsd.prog.mk>
