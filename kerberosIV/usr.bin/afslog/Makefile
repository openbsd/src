# $OpenBSD: Makefile,v 1.3 2001/01/09 02:41:19 angelos Exp $

PROG	= afslog

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/appl/afsutil

CFLAGS	+= -I.
LDADD	+= ${LIBROKEN} -lkafs -lkrb -ldes

SRCS	= aklog.c

.include <bsd.prog.mk>
