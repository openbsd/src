# $OpenBSD: Makefile,v 1.2 2000/03/01 08:55:09 deraadt Exp $

PROG	= kdb_util

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/admin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkafs -lkdb -lkrb -ldes

SRCS	= kdb_util.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
