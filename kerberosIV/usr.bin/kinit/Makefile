# $OpenBSD: Makefile,v 1.3 2002/05/11 00:20:18 espie Exp $

PROG	= kinit

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/kuser

CFLAGS	+= -I.
DPADD	+= ${LIBKRB} ${LIBKAFS} ${LIBDES}
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes

SRCS	= kinit.c

.include <bsd.prog.mk>
