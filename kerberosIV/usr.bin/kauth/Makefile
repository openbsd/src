# $OpenBSD: Makefile,v 1.1 2000/02/25 16:35:29 hin Exp $

PROG	= kauth

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/appl/kauth

CFLAGS	+= -I.
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes

SRCS	= kauth.c rkinit.c encdata.c marshall.c

.include <bsd.prog.mk>
