#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
#	$Id: Makefile,v 1.1 1996/03/02 03:01:42 dm Exp $

PROG=	yppush
SRCS=	yppush.c yplib_host.c yppush_svc.c yppush_xdr.c yppush_proc.c ypdb.c yppush_err.c
MAN=	yppush.8
.PATH:	${.CURDIR}/../common ${.CURDIR}/../ypserv
CFLAGS+=-I${.CURDIR}/../common

.include <bsd.prog.mk>
