# $OpenBSD: Makefile,v 1.1 2000/02/25 16:35:33 hin Exp $

PROG	= kdb_destroy

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/admin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes -lkdb

SRCS	= kdb_destroy.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
