# $OpenBSD: Makefile,v 1.1 2000/02/25 16:35:31 hin Exp $

PROG	= kstring2key

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/appl/afsutil

CFLAGS	+= -I.
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes

SRCS	= kstring2key.c

NOMAN	=

.include <bsd.prog.mk>
