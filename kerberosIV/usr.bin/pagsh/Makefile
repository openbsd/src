# $OpenBSD: Makefile,v 1.3 2001/01/09 02:41:20 angelos Exp $

PROG	= pagsh

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/appl/afsutil

CFLAGS	+= -I.
LDADD	+= ${LIBROKEN} -lkrb -lkafs -lcrypto

SRCS	= pagsh.c

.include <bsd.prog.mk>
