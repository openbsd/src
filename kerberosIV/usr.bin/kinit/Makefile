# $OpenBSD: Makefile,v 1.2 2001/01/09 02:41:20 angelos Exp $

PROG	= kinit

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/kuser

CFLAGS	+= -I.
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes

SRCS	= kinit.c

.include <bsd.prog.mk>
