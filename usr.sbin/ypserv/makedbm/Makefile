#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
#	$Id: Makefile,v 1.1 1995/11/01 16:56:22 deraadt Exp $

PROG=	makedbm
SRCS=	makedbm.c ypdb.c
MAN=	makedbm.8
.PATH:	${.CURDIR}/../common
CFLAGS+=-I${.CURDIR}/../common

.include <bsd.prog.mk>
