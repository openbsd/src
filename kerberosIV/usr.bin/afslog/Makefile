# $OpenBSD: Makefile,v 1.1 2000/02/25 16:35:29 hin Exp $

PROG	= afslog

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/appl/afsutil

CFLAGS	+= -I.
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes

SRCS	= aklog.c

.include <bsd.prog.mk>
