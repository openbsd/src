# $OpenBSD: Makefile,v 1.2 2000/09/16 18:26:10 rahnds Exp $

PROG	= pagsh

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/appl/afsutil

CFLAGS	+= -I.
LDADD	+= ${LIBROKEN} -lkrb -lkafs -lcrypto

SRCS	= pagsh.c

.include <bsd.prog.mk>
