# $OpenBSD: Makefile,v 1.2 2002/05/11 00:20:19 espie Exp $

PROG	= kstring2key

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/appl/afsutil

CFLAGS	+= -I.
DPADD	+= ${LIBKRB} ${LIBKAFS} ${LIBDES}
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes

SRCS	= kstring2key.c

NOMAN	=

.include <bsd.prog.mk>
