# $OpenBSD: Makefile,v 1.3 2001/01/09 02:41:20 angelos Exp $

PROG	= klist

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/kuser

CFLAGS	+= -I.
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes

SRCS	= klist.c

.include <bsd.prog.mk>
