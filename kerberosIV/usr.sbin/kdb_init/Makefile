# $OpenBSD: Makefile,v 1.1 2000/02/25 16:35:34 hin Exp $

PROG	= kdb_init

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/admin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes -lkdb

SRCS	= kdb_init.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
