# $OpenBSD: Makefile,v 1.2 2000/03/01 08:55:06 deraadt Exp $

PROG	= afslog

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/appl/afsutil

CFLAGS	+= -I.
LDADD	+= ${LIBROKEN} -lkafs -lkrb -ldes

SRCS	= aklog.c

.include <bsd.prog.mk>
