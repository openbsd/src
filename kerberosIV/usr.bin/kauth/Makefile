# $OpenBSD: Makefile,v 1.3 2002/05/11 00:20:18 espie Exp $

PROG	= kauth

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/appl/kauth

CFLAGS	+= -I.
DPADD	+= ${LIBKRB} ${LIBKAFS} ${LIBDES}
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes

SRCS	= kauth.c rkinit.c encdata.c marshall.c

.include <bsd.prog.mk>
