#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$OpenBSD: Makefile,v 1.3 2002/05/20 23:13:50 millert Exp $

PROG=	pac
CFLAGS+=-I${.CURDIR}/../common_source
MAN=	pac.8
SRCS=	pac.c common.c common_vars.c
.PATH:	${.CURDIR}/../common_source

.include "../../Makefile.inc"
.include <bsd.prog.mk>
