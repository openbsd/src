# $OpenBSD: Makefile,v 1.2 2000/03/27 11:31:58 maja Exp $

PROG	= klist

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/kuser

CFLAGS	+= -I.
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes

SRCS	= klist.c

.include <bsd.prog.mk>
