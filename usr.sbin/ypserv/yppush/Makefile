#	$OpenBSD: Makefile,v 1.2 1996/05/30 09:53:18 deraadt Exp $

PROG=	yppush
SRCS=	yppush.c yplib_host.c yppush_svc.c yppush_xdr.c yppush_proc.c ypdb.c yppush_err.c
MAN=	yppush.8
.PATH:	${.CURDIR}/../common ${.CURDIR}/../ypserv
CFLAGS+=-I${.CURDIR}/../common

.include <bsd.prog.mk>
