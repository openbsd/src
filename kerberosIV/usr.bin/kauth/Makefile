# $OpenBSD: Makefile,v 1.2 2001/01/09 02:41:19 angelos Exp $

PROG	= kauth

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/appl/kauth

CFLAGS	+= -I.
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes

SRCS	= kauth.c rkinit.c encdata.c marshall.c

.include <bsd.prog.mk>
