#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$OpenBSD: Makefile,v 1.2 1997/01/17 16:12:49 millert Exp $

PROG=	pac
CFLAGS+=-I${.CURDIR}/../common_source
MAN=	pac.8
SRCS=	pac.c common.c
.PATH:	${.CURDIR}/../common_source

.include "../../Makefile.inc"
.include <bsd.prog.mk>
