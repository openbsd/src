# $OpenBSD: Makefile,v 1.4 2002/05/11 00:20:18 espie Exp $

PROG	= kdestroy

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/kuser

CFLAGS	+= -I.
DPADD	+= ${LIBKRB} ${LIBKAFS} ${LIBDES}
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes

SRCS	= kdestroy.c

.include <bsd.prog.mk>
