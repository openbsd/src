# $OpenBSD: Makefile,v 1.2 2000/03/01 08:55:08 deraadt Exp $

PROG	= kdb_edit

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/admin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkafs -lkdb -lkrb -ldes

SRCS	= kdb_edit.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
