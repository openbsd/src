# $OpenBSD: Makefile,v 1.1 2000/02/25 16:35:29 hin Exp $

PROG	= kdestroy

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/kuser

CFLAGS	+= -I.
LDADD	+= ${LIBROKEN} -lkrb -lkafs

SRCS	= kdestroy.c

.include <bsd.prog.mk>
