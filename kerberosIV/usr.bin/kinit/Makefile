# $OpenBSD: Makefile,v 1.1 2000/02/25 16:35:30 hin Exp $

PROG	= kinit

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/kuser

CFLAGS	+= -I.
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes

SRCS	= kinit.c

.include <bsd.prog.mk>
