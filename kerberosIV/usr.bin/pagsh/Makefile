# $OpenBSD: Makefile,v 1.1 2000/02/25 16:35:32 hin Exp $

PROG	= pagsh

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/appl/afsutil

CFLAGS	+= -I.
LDADD	+= ${LIBROKEN} -lkrb -lkafs

SRCS	= pagsh.c

.include <bsd.prog.mk>
