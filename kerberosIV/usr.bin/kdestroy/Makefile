# $OpenBSD: Makefile,v 1.2 2000/03/27 11:31:57 maja Exp $

PROG	= kdestroy

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/kuser

CFLAGS	+= -I.
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes

SRCS	= kdestroy.c

.include <bsd.prog.mk>
